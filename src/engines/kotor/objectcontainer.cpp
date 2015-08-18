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
 *  A container of Star Wars: Knights of the Old Republic objects.
 */

#include "src/common/types.h"
#include "src/common/util.h"

#include "src/engines/kotor/objectcontainer.h"
#include "src/engines/kotor/object.h"
#include "src/engines/kotor/module.h"
#include "src/engines/kotor/area.h"
#include "src/engines/kotor/waypoint.h"
#include "src/engines/kotor/situated.h"
#include "src/engines/kotor/placeable.h"
#include "src/engines/kotor/door.h"
#include "src/engines/kotor/creature.h"

namespace Engines {

namespace KotOR {

ObjectDistanceSort::ObjectDistanceSort(const KotOR::Object &target) {
	target.getPosition(xt, yt, zt);
}

bool ObjectDistanceSort::operator()(KotOR::Object *a, KotOR::Object *b) {
	return getDistance(*a) < getDistance(*b);
}

float ObjectDistanceSort::getDistance(KotOR::Object &a) {
	float x, y, z;
	a.getPosition(x, y, z);

	return ABS(x - xt) + ABS(y - yt) + ABS(z - zt);
}


class SearchType : public ::Aurora::NWScript::SearchRange< std::list<KotOR::Object *> > {
public:
	SearchType(const iterator &a, const iterator &b) : ::Aurora::NWScript::SearchRange<type>(std::make_pair(a, b)) { }
	SearchType(const type &l) : ::Aurora::NWScript::SearchRange<type>(std::make_pair(l.begin(), l.end())) { }
	~SearchType() { }

	::Aurora::NWScript::Object *getObject(const iterator &t) { return *t; }
};


ObjectContainer::ObjectContainer() {
}

ObjectContainer::~ObjectContainer() {
}

void ObjectContainer::clearObjects() {
	lock();

	for (size_t i = 0; i < kObjectTypeMAX; i++)
		_objects[i].clear();

	::Aurora::NWScript::ObjectContainer::clearObjects();

	unlock();
}

void ObjectContainer::addObject(KotOR::Object &object) {
	lock();

	::Aurora::NWScript::ObjectContainer::addObject(object);

	ObjectType type = object.getType();
	if (((uint) type) < kObjectTypeMAX)
		_objects[type].push_back(&object);

	unlock();
}

void ObjectContainer::removeObject(KotOR::Object &object) {
	lock();

	ObjectType type = object.getType();
	if (((uint) type) < kObjectTypeMAX)
		_objects[type].remove(&object);

	::Aurora::NWScript::ObjectContainer::removeObject(object);

	unlock();
}

::Aurora::NWScript::Object *ObjectContainer::getFirstObjectByType(ObjectType type) const {
	if (((uint) type) >= kObjectTypeMAX)
		return 0;

	SearchType ctx(_objects[type]);

	return ctx.get();
}

::Aurora::NWScript::ObjectSearch *ObjectContainer::findObjectsByType(ObjectType type) const {
	if (((uint) type) >= kObjectTypeMAX)
		return new SearchType(_objects[0].end(), _objects[0].end());

	return new SearchType(_objects[type]);
}

KotOR::Object *ObjectContainer::toObject(::Aurora::NWScript::Object *object) {
	return dynamic_cast<KotOR::Object *>(object);
}

Module *ObjectContainer::toModule(Aurora::NWScript::Object *object) {
	return dynamic_cast<Module *>(object);
}

Area *ObjectContainer::toArea(Aurora::NWScript::Object *object) {
	return dynamic_cast<Area *>(object);
}

Waypoint *ObjectContainer::toWaypoint(Aurora::NWScript::Object *object) {
	return dynamic_cast<Waypoint *>(object);
}

Situated *ObjectContainer::toSituated(Aurora::NWScript::Object *object) {
	return dynamic_cast<Situated *>(object);
}

Placeable *ObjectContainer::toPlaceable(Aurora::NWScript::Object *object) {
	return dynamic_cast<Placeable *>(object);
}

Door *ObjectContainer::toDoor(Aurora::NWScript::Object *object) {
	return dynamic_cast<Door *>(object);
}

Creature *ObjectContainer::toCreature(Aurora::NWScript::Object *object) {
	return dynamic_cast<Creature *>(object);
}

Creature *ObjectContainer::toPC(Aurora::NWScript::Object *object) {
	Creature *pc = dynamic_cast<Creature *>(object);
	if (!pc || !pc->isPC())
		return 0;

	return pc;
}

} // End of namespace KotOR

} // End of namespace Engines