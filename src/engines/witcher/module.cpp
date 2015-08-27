/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  The context needed to run a The Witcher module.
 */

#include "src/common/util.h"
#include "src/common/maths.h"
#include "src/common/error.h"
#include "src/common/configman.h"
#include "src/common/readfile.h"
#include "src/common/filepath.h"
#include "src/common/filelist.h"

#include "src/aurora/resman.h"
#include "src/aurora/erffile.h"
#include "src/aurora/gff3file.h"

#include "src/graphics/camera.h"

#include "src/events/events.h"

#include "src/engines/aurora/util.h"
#include "src/engines/aurora/resources.h"
#include "src/engines/aurora/console.h"

#include "src/engines/witcher/module.h"
#include "src/engines/witcher/area.h"

namespace Engines {

namespace Witcher {

Module::Module(::Engines::Console &console) : Object(kObjectTypeModule), _console(&console),
	_hasModule(false), _running(false), _exit(false), _currentArea(0) {

}

Module::~Module() {
	try {
		clear();
	} catch (...) {
	}
}

void Module::clear() {
	unload();
}

Area *Module::getCurrentArea() {
	return _currentArea;
}

bool Module::isLoaded() const {
	return _hasModule;
}

bool Module::isRunning() const {
	return !EventMan.quitRequested() && _running && !_exit && !_newArea.empty();
}

void Module::load(const Common::UString &module) {
	if (isRunning()) {
		// We are currently running a module. Schedule a safe change instead

		changeModule(module);
		return;
	}

	// We are not currently running a module. Directly load the new module
	loadModule(module);
}

void Module::loadModule(const Common::UString &module) {
	unload();

	if (module.empty())
		throw Common::Exception("Tried to load an empty module");

	try {
		indexMandatoryArchive(module, 1001, &_resModule);

		_ifo.load();

		if (_ifo.isSave())
			throw Common::Exception("This is a save");

		_tag  = _ifo.getTag();
		_name = _ifo.getName();

	} catch (Common::Exception &e) {
		e.add("Can't load module \"%s\"", module.c_str());
		throw e;
	}

	_newModule.clear();

	_hasModule = true;
}

void Module::changeModule(const Common::UString &module) {
	_newModule = module;
}

void Module::replaceModule() {
	if (_newModule.empty())
		return;

	_console->hide();

	Common::UString newModule = _newModule;

	unload();

	_exit = true;

	loadModule(newModule);
	enter();
}

void Module::enter() {
	if (!isLoaded())
		throw Common::Exception("Module::enter(): Lacking a module?!?");

	try {

		loadAreas();

	} catch (Common::Exception &e) {
		e.add("Can't initialize module \"%s\"", _name.getString().c_str());
		throw e;
	}

	float entryX, entryY, entryZ, entryDirX, entryDirY;
	_ifo.getEntryPosition(entryX, entryY, entryZ);
	_ifo.getEntryDirection(entryDirX, entryDirY);

	const float entryAngle = -Common::rad2deg(atan2(entryDirX, entryDirY));

	_console->printf("Entering module \"%s\"", _name.getString().c_str());

	Common::UString startMovie = _ifo.getStartMovie();
	if (!startMovie.empty())
		playVideo(startMovie);

	_newArea = _ifo.getEntryArea();

	CameraMan.reset();

	// Roughly head position
	CameraMan.setPosition(entryX, entryY, entryZ + 1.8f);
	CameraMan.setOrientation(90.0f, 0.0f, entryAngle);
	CameraMan.update();

	_running = true;
	_exit    = false;
}

void Module::leave() {
	_running = false;
	_exit    = true;
}

void Module::addEvent(const Events::Event &event) {
	_eventQueue.push_back(event);
}

void Module::processEventQueue() {
	if (!isRunning())
		return;

	replaceModule();
	enterArea();

	if (!isRunning())
		return;

	handleEvents();
}

void Module::enterArea() {
	if (_currentArea && (_currentArea->getResRef() == _newArea))
		return;

	if (_currentArea) {
		_currentArea->hide();

		_currentArea = 0;
	}

	if (_newArea.empty()) {
		_exit = true;
		return;
	}

	AreaMap::iterator area = _areas.find(_newArea);
	if (area == _areas.end() || !area->second) {
		warning("Failed entering area \"%s\": No such area", _newArea.c_str());
		_exit = true;
		return;
	}

	_currentArea = area->second;

	_currentArea->show();

	EventMan.flushEvents();

	_console->printf("Entering area \"%s\" (\"%s\")", _currentArea->getResRef().c_str(),
			_currentArea->getName().getString().c_str());
}

void Module::handleEvents() {
	for (EventQueue::const_iterator event = _eventQueue.begin(); event != _eventQueue.end(); ++event)
		_currentArea->addEvent(*event);

	_eventQueue.clear();

	_currentArea->processEventQueue();
}

void Module::unload() {
	unloadAreas();
	unloadModule();
}

void Module::unloadModule() {
	_tag.clear();

	_ifo.unload();

	deindexResources(_resModule);

	_newModule.clear();

	_eventQueue.clear();

	_hasModule = false;
	_running   = false;
	_exit      = true;
}

void Module::loadAreas() {
	status("Loading areas...");

	const std::vector<Common::UString> &areas = _ifo.getAreas();
	for (size_t i = 0; i < areas.size(); i++) {
		status("Loading area \"%s\" (%d / %d)", areas[i].c_str(), (int)i, (int)areas.size() - 1);

		std::pair<AreaMap::iterator, bool> result;

		result = _areas.insert(std::make_pair(areas[i], (Area *) 0));
		if (!result.second)
			throw Common::Exception("Area tag collision: \"%s\"", areas[i].c_str());

		try {
			result.first->second = new Area(*this, areas[i].c_str());
		} catch (Common::Exception &e) {
			e.add("Can't load area \"%s\"", areas[i].c_str());
			throw;
		}
	}
}

void Module::unloadAreas() {
	for (AreaMap::iterator a = _areas.begin(); a != _areas.end(); ++a)
		delete a->second;

	_areas.clear();
	_newArea.clear();

	_currentArea = 0;
}

void Module::movePC(const Common::UString &area) {
	_newArea = area;
}

void Module::movePC(float x, float y, float z) {
	// Roughly head position
	CameraMan.setPosition(x, y, z + 1.8f);
	CameraMan.update();
}

void Module::movePC(const Common::UString &area, float x, float y, float z) {
	movePC(area);
	movePC(x, y, z);
}

const Aurora::IFOFile &Module::getIFO() const {
	return _ifo;
}

const Aurora::LocString &Module::getName() const {
	return Witcher::Object::getName();
}

const Aurora::LocString &Module::getDescription() const {
	return Witcher::Object::getDescription();
}

void Module::refreshLocalized() {
	for (AreaMap::iterator a = _areas.begin(); a != _areas.end(); ++a)
		a->second->refreshLocalized();
}

Common::UString Module::getName(const Common::UString &module) {
	try {
		const Aurora::ERFFile mod(new Common::ReadFile(findModule(module, false)));
		const uint32 ifoIndex = mod.findResource("module", Aurora::kFileTypeIFO);

		const Aurora::GFF3File ifo(mod.getResource(ifoIndex), MKTAG('I', 'F', 'O', ' '));

		return ifo.getTopLevel().getString("Mod_Name");

	} catch (...) {
	}

	return "";
}

Common::UString Module::getDescription(const Common::UString &module) {
	try {
		const Aurora::ERFFile mod(new Common::ReadFile(findModule(module, false)));
		const uint32 ifoIndex = mod.findResource("module", Aurora::kFileTypeIFO);

		const Aurora::GFF3File ifo(mod.getResource(ifoIndex), MKTAG('I', 'F', 'O', ' '));

		return ifo.getTopLevel().getString("Mod_Description");

	} catch (...) {
	}

	return "";
}

Common::UString Module::findModule(const Common::UString &module, bool relative) {
	const Common::FileList modFiles(ConfigMan.getString("WITCHER_moduleDir"), -1);

	for (Common::FileList::const_iterator m = modFiles.begin(); m != modFiles.end(); ++m) {
		if (!Common::FilePath::getFile(*m).equalsIgnoreCase(module + ".mod") &&
		    !Common::FilePath::getFile(*m).equalsIgnoreCase(module + ".adv"))
			continue;

		if (!relative)
			return *m;

		return Common::FilePath::relativize(ResMan.getDataBase(), *m);
	}

	return "";
}

} // End of namespace Witcher

} // End of namespace Engines
