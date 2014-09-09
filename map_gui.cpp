/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"

#include "map_gui.h"
#include "view_object.h"
#include "map_layout.h"
#include "view_index.h"
#include "tile.h"
#include "window_view.h"
#include "window_renderer.h"
#include "clock.h"
#include "view_id.h"
#include "level.h"


MapGui::MapGui(const Table<Optional<ViewIndex>>& o, function<void(Vec2)> fun)
    : objects(o), leftClickFun(fun), fogOfWar(Level::getMaxBounds(), false) {
}

static int fireVar = 50;

static Color getFireColor() {
  return Color(200 + Random.getRandom(-fireVar, fireVar), Random.getRandom(fireVar), Random.getRandom(fireVar), 150);
}


void MapGui::setLayout(MapLayout* l) {
  layout = l;
}

void MapGui::setSpriteMode(bool s) {
  spriteMode = s;
}

void MapGui::addAnimation(PAnimation animation, Vec2 pos) {
  animation->setBegin(Clock::get().getRealMillis());
  animations.push_back({std::move(animation), pos});
}


Optional<Vec2> MapGui::getHighlightedTile(WindowRenderer& renderer) {
  Vec2 pos = renderer.getMousePos();
  if (!pos.inRectangle(getBounds()))
    return Nothing();
  return layout->projectOnMap(getBounds(), pos);
}

static Color getBleedingColor(const ViewObject& object) {
  double bleeding = object.getAttribute(ViewObject::Attribute::BLEEDING);
 /* if (object.isPoisoned())
    return Color(0, 255, 0);*/
  if (bleeding > 0)
    bleeding = 0.3 + bleeding * 0.7;
  return Color(255, max(0., (1 - bleeding) * 255), max(0., (1 - bleeding) * 255));
}

Color getHighlightColor(HighlightType type, double amount) {
  switch (type) {
    case HighlightType::BUILD: return transparency(colors[ColorId::YELLOW], 170);
    case HighlightType::RECT_SELECTION: return transparency(colors[ColorId::YELLOW], 90);
    case HighlightType::FOG: return transparency(colors[ColorId::WHITE], 120 * amount);
    case HighlightType::POISON_GAS: return Color(0, min(255., amount * 500), 0, amount * 140);
    case HighlightType::MEMORY: return transparency(colors[ColorId::BLACK], 80);
    case HighlightType::NIGHT: return transparency(colors[ColorId::NIGHT_BLUE], amount * 160);
    case HighlightType::EFFICIENCY: return transparency(Color(255, 0, 0) , 120 * (1 - amount));
  }
  FAIL << "pokpok";
  return Color();
}

enum class ConnectionId {
  ROAD,
  WALL,
  WATER,
  MOUNTAIN2,
  LIBRARY,
  TRAINING_ROOM,
  TORTURE_ROOM,
  WORKSHOP,
  LABORATORY,
  BRIDGE,
};

unordered_map<Vec2, ConnectionId> floorIds;
set<Vec2> shadowed;

Optional<ConnectionId> getConnectionId(const ViewObject& object) {
  if (object.hasModifier(ViewObject::Modifier::PLANNED))
    return Nothing();
  switch (object.id()) {
    case ViewId::ROAD: return ConnectionId::ROAD;
    case ViewId::BLACK_WALL:
    case ViewId::YELLOW_WALL:
    case ViewId::HELL_WALL:
    case ViewId::LOW_ROCK_WALL:
    case ViewId::WOOD_WALL:
    case ViewId::CASTLE_WALL:
    case ViewId::MUD_WALL:
    case ViewId::MOUNTAIN2:
    case ViewId::WALL: return ConnectionId::WALL;
    case ViewId::MAGMA:
    case ViewId::WATER: return ConnectionId::WATER;
    case ViewId::LIBRARY: return ConnectionId::LIBRARY;
    case ViewId::TRAINING_ROOM: return ConnectionId::TRAINING_ROOM;
    case ViewId::TORTURE_TABLE: return ConnectionId::TORTURE_ROOM;
    case ViewId::WORKSHOP: return ConnectionId::WORKSHOP;
    case ViewId::LABORATORY: return ConnectionId::LABORATORY;
    case ViewId::BRIDGE: return ConnectionId::BRIDGE;
    default: return Nothing();
  }
}

vector<Vec2>& getConnectionDirs(ViewId id) {
  static vector<Vec2> v4 = Vec2::directions4();
  static vector<Vec2> v8 = Vec2::directions8();
  switch (id) {
    case ViewId::LIBRARY:
    case ViewId::WORKSHOP:
    case ViewId::LABORATORY:
    case ViewId::TORTURE_TABLE:
    case ViewId::TRAINING_ROOM: return v8;
    default: return v4;
  }
}

bool tileConnects(ConnectionId id, Vec2 pos) {
  return floorIds.count(pos) && id == floorIds.at(pos);
}

void MapGui::onLeftClick(Vec2 v) {
  if (optionsGui && v.inRectangle(optionsGui->getBounds()))
    optionsGui->onLeftClick(v);
  else if (v.inRectangle(getBounds())) {
    Vec2 pos = layout->projectOnMap(getBounds(), v);
    leftClickFun(pos);
    mouseHeldPos = pos;
  }
}

void MapGui::onRightClick(Vec2) {
}

void MapGui::onMouseMove(Vec2 v) {
  if (optionsGui)
    optionsGui->onMouseMove(v);
  Vec2 pos = layout->projectOnMap(getBounds(), v);
  if (v.inRectangle(getBounds()) && (!optionsGui || !v.inRectangle(optionsGui->getBounds()))) {
    if (mouseHeldPos && *mouseHeldPos != pos) {
      leftClickFun(pos);
      mouseHeldPos = pos;
    }
    highlightedPos = pos;
  } else
    highlightedPos = Nothing();
}

void MapGui::onMouseRelease() {
  mouseHeldPos = Nothing();
}

void MapGui::drawObjectAbs(Renderer& renderer, int x, int y, const ViewObject& object,
    int sizeX, int sizeY, Vec2 tilePos) {
  if (object.hasModifier(ViewObject::Modifier::PLAYER)) {
    renderer.drawFilledRectangle(x, y, x + sizeX, y + sizeY, Color::Transparent, colors[ColorId::LIGHT_GRAY]);
  }
  if (object.hasModifier(ViewObject::Modifier::TEAM_HIGHLIGHT)) {
    renderer.drawFilledRectangle(x, y, x + sizeX, y + sizeY, Color::Transparent, colors[ColorId::DARK_GREEN]);
  }
  Tile tile = Tile::getTile(object, spriteMode);
  Color color = getBleedingColor(object);
  if (object.hasModifier(ViewObject::Modifier::INVISIBLE) || object.hasModifier(ViewObject::Modifier::HIDDEN))
    color = transparency(color, 70);
  else
    if (tile.translucent > 0)
      color = transparency(color, 255 * (1 - tile.translucent));
    else if (object.hasModifier(ViewObject::Modifier::ILLUSION))
      color = transparency(color, 150);
  if (object.hasModifier(ViewObject::Modifier::PLANNED))
    color = transparency(color, 100);
  double waterDepth = object.getAttribute(ViewObject::Attribute::WATER_DEPTH);
  if (waterDepth > 0) {
    int val = max(0.0, 255.0 - min(2.0, waterDepth) * 60);
    color = Color(val, val, val);
  }
  if (tile.hasSpriteCoord()) {
    int moveY = 0;
    Vec2 sz = Renderer::tileSize[tile.getTexNum()];
    Vec2 off = (Renderer::nominalSize -  sz).mult(Vec2(sizeX, sizeY)).div(Renderer::nominalSize * 2);
    int width = sz.x * sizeX / Renderer::nominalSize.x;
    int height = sz.y* sizeY / Renderer::nominalSize.y;
    if (sz.y > Renderer::nominalSize.y)
      off.y *= 2;
    EnumSet<Dir> dirs;
    if (!object.hasModifier(ViewObject::Modifier::PLANNED))
      if (auto connectionId = getConnectionId(object))
        for (Vec2 dir : getConnectionDirs(object.id()))
          if (tileConnects(*connectionId, tilePos + dir))
            dirs.insert(dir.getCardinalDir());
    Vec2 coord = tile.getSpriteCoord(dirs);
    if (object.hasModifier(ViewObject::Modifier::MOVE_UP))
      moveY = -4* sizeY / Renderer::nominalSize.y;
    if (object.layer() == ViewLayer::CREATURE || object.hasModifier(ViewObject::Modifier::ROUND_SHADOW)) {
      renderer.drawSprite(x, y - 2, 2 * Renderer::nominalSize.x, 22 * Renderer::nominalSize.y,
          Renderer::nominalSize.x, Renderer::nominalSize.y, Renderer::tiles[0],
          min(Renderer::nominalSize.x, width), min(Renderer::nominalSize.y, height));
      moveY = -4* sizeY / Renderer::nominalSize.y;
    }
    if (auto background = tile.getBackgroundCoord()) {
      renderer.drawSprite(x + off.x, y + off.y, background->x * sz.x,
          background->y * sz.y, sz.x, sz.y, Renderer::tiles[tile.getTexNum()], width, height, color);
      if (shadowed.count(tilePos))
        renderer.drawSprite(x, y, 1 * Renderer::nominalSize.x, 21 * Renderer::nominalSize.y,
            Renderer::nominalSize.x, Renderer::nominalSize.y, Renderer::tiles[5], width, height);
    }
    if (coord.x < 0)
      return;
    renderer.drawSprite(x + off.x, y + moveY + off.y, coord.x * sz.x,
        coord.y * sz.y, sz.x, sz.y, Renderer::tiles[tile.getTexNum()], width, height, color);
    if (contains({ViewLayer::FLOOR, ViewLayer::FLOOR_BACKGROUND}, object.layer()) && 
        shadowed.count(tilePos) && !tile.noShadow)
      renderer.drawSprite(x, y, 1 * Renderer::nominalSize.x, 21 * Renderer::nominalSize.y,
          Renderer::nominalSize.x, Renderer::nominalSize.y, Renderer::tiles[5], width, height);
    if (object.getAttribute(ViewObject::Attribute::BURNING) > 0) {
      renderer.drawSprite(x, y, Random.getRandom(10, 12) * Renderer::nominalSize.x, 0 * Renderer::nominalSize.y,
          Renderer::nominalSize.x, Renderer::nominalSize.y, Renderer::tiles[2], width, height);
    }
    if (object.hasModifier(ViewObject::Modifier::LOCKED))
      renderer.drawSprite(x + (Renderer::nominalSize.x - Renderer::tileSize[3].x) / 2, y,
          5 * Renderer::tileSize[3].x, 6 * Renderer::tileSize[3].y,
          Renderer::tileSize[3].x, Renderer::tileSize[3].y, Renderer::tiles[3], width / 2, height / 2);
  } else {
    renderer.drawText(tile.symFont ? Renderer::SYMBOL_FONT : Renderer::TILE_FONT, sizeY, Tile::getColor(object),
        x + sizeX / 2, y - 3, tile.text, true);
    double burningVal = object.getAttribute(ViewObject::Attribute::BURNING);
    if (burningVal > 0) {
      renderer.drawText(Renderer::SYMBOL_FONT, sizeY, getFireColor(), x + sizeX / 2, y - 3, L'ѡ', true);
      if (burningVal > 0.5)
        renderer.drawText(Renderer::SYMBOL_FONT, sizeY, getFireColor(), x + sizeX / 2, y - 3, L'Ѡ', true);
    }
  }
}

void MapGui::setLevelBounds(Rectangle b) {
  levelBounds = b;
}

void MapGui::updateObjects(const MapMemory* mem) {
  lastMemory = mem;
  floorIds.clear();
  shadowed.clear();
  for (Vec2 wpos : layout->getAllTiles(getBounds(), objects.getBounds()))
    if (auto& index = objects[wpos])
      if (index->hasObject(ViewLayer::FLOOR)) {
        ViewObject object = index->getObject(ViewLayer::FLOOR);
        if (object.hasModifier(ViewObject::Modifier::CASTS_SHADOW)) {
          shadowed.erase(wpos);
          shadowed.insert(wpos + Vec2(0, 1));
        }
        if (auto id = getConnectionId(object))
          floorIds.insert(make_pair(wpos, *id));
      }
}

const int bgTransparency = 180;

void MapGui::drawHint(Renderer& renderer, Color color, const string& text) {
  int height = 30;
  int width = renderer.getTextLength(text) + 30;
  Vec2 pos(getBounds().getKX() - width, getBounds().getKY() - height);
  renderer.drawFilledRectangle(pos.x, pos.y, pos.x + width, pos.y + height, transparency(colors[ColorId::BLACK],
        bgTransparency));
  renderer.drawText(color, pos.x + 10, pos.y + 1, text);
}

void MapGui::drawFoWSprite(Renderer& renderer, Vec2 pos, int sizeX, int sizeY, EnumSet<Dir> dirs) {
  const Tile& tile = Tile::fromViewId(ViewId::FOG_OF_WAR); 
  const Tile& tile2 = Tile::fromViewId(ViewId::FOG_OF_WAR_CORNER); 
  Vec2 coord = tile.getSpriteCoord(dirs.intersection({Dir::N, Dir::S, Dir::E, Dir::W}));
  Vec2 sz = Renderer::tileSize[tile.getTexNum()];
  renderer.drawSprite(pos.x, pos.y, coord.x * sz.x, coord.y * sz.y, sz.x, sz.y,
      Renderer::tiles[tile.getTexNum()], sizeX, sizeY);
  for (Dir dir : dirs.intersection({Dir::NE, Dir::SE, Dir::NW, Dir::SW})) {
    switch (dir) {
      case Dir::NE: if (!dirs[Dir::N] || !dirs[Dir::E]) continue;
      case Dir::SE: if (!dirs[Dir::S] || !dirs[Dir::E]) continue;
      case Dir::NW: if (!dirs[Dir::N] || !dirs[Dir::W]) continue;
      case Dir::SW: if (!dirs[Dir::S] || !dirs[Dir::W]) continue;
      default: break;
    }
    Vec2 coord = tile2.getSpriteCoord({dir});
    renderer.drawSprite(pos.x, pos.y, coord.x * sz.x, coord.y * sz.y, sz.x, sz.y,
        Renderer::tiles[tile2.getTexNum()], sizeX, sizeY);
  }
}

bool MapGui::isFoW(Vec2 pos) const {
  return !pos.inRectangle(Level::getMaxBounds()) || fogOfWar.getValue(pos);
}

void MapGui::render(Renderer& renderer) {
  int sizeX = layout->squareWidth();
  int sizeY = layout->squareHeight();
  renderer.drawFilledRectangle(getBounds(), colors[ColorId::ALMOST_BLACK]);
  Optional<ViewObject> highlighted;
  fogOfWar.clear();
  for (ViewLayer layer : layout->getLayers()) {
    for (Vec2 wpos : layout->getAllTiles(getBounds(), levelBounds)) {
      Vec2 pos = layout->projectOnScreen(getBounds(), wpos);
      if (!spriteMode && wpos.inRectangle(levelBounds))
        renderer.drawFilledRectangle(pos.x, pos.y, pos.x + sizeX, pos.y + sizeY, colors[ColorId::BLACK]);
      if (!objects[wpos] || objects[wpos]->noObjects()) {
        if (layer == layout->getLayers().back()) {
          if (wpos.inRectangle(levelBounds))
            renderer.drawFilledRectangle(pos.x, pos.y, pos.x + sizeX, pos.y + sizeY, colors[ColorId::BLACK]);
        }
        fogOfWar.setValue(wpos, true);
        continue;
      }
      const ViewIndex& index = *objects[wpos];
      const ViewObject* object = nullptr;
      if (spriteMode) {
        if (index.hasObject(layer))
          object = &index.getObject(layer);
      } else
        object = index.getTopObject(layout->getLayers());
      if (object) {
        drawObjectAbs(renderer, pos.x, pos.y, *object, sizeX, sizeY, wpos);
        if (highlightedPos == wpos)
          highlighted = *object;
      }
      if (contains({ViewLayer::FLOOR, ViewLayer::FLOOR_BACKGROUND}, layer) && highlightedPos == wpos) {
        renderer.drawFilledRectangle(pos.x, pos.y, pos.x + sizeX, pos.y + sizeY, Color::Transparent,
            colors[ColorId::LIGHT_GRAY]);
      }
      if (layer == layout->getLayers().back())
        if (!isFoW(wpos))
          drawFoWSprite(renderer, pos, sizeX, sizeY, {
              !isFoW(wpos + Vec2(Dir::N)),
              !isFoW(wpos + Vec2(Dir::S)),
              !isFoW(wpos + Vec2(Dir::E)),
              !isFoW(wpos + Vec2(Dir::W)),
              isFoW(wpos + Vec2(Dir::NE)),
              isFoW(wpos + Vec2(Dir::NW)),
              isFoW(wpos + Vec2(Dir::SE)),
              isFoW(wpos + Vec2(Dir::SW))});
    }
    if (!spriteMode)
      break;
  }
  for (Vec2 wpos : layout->getAllTiles(getBounds(), levelBounds))
    if (auto index = objects[wpos]) {
      Vec2 pos = layout->projectOnScreen(getBounds(), wpos);
      for (HighlightType highlight : ENUM_ALL(HighlightType))
        if (index->getHighlight(highlight) > 0)
          renderer.drawFilledRectangle(pos.x, pos.y, pos.x + sizeX, pos.y + sizeY,
              getHighlightColor(highlight, index->getHighlight(highlight)));
      if (highlightedPos == wpos) {
        renderer.drawFilledRectangle(pos.x, pos.y, pos.x + sizeX, pos.y + sizeY, Color::Transparent,
            colors[ColorId::LIGHT_GRAY]);
      }
    }
  animations = filter(std::move(animations), [](const AnimationInfo& elem) 
      { return !elem.animation->isDone(Clock::get().getRealMillis());});
  for (auto& elem : animations)
    elem.animation->render(
        renderer,
        getBounds(),
        layout->projectOnScreen(getBounds(), elem.position),
        Clock::get().getRealMillis());
  if (!hint.empty())
    drawHint(renderer, colors[ColorId::WHITE], hint);
  else
  if (highlightedPos && highlighted) {
    Color col = colors[ColorId::WHITE];
    if (highlighted->isHostile())
      col = colors[ColorId::RED];
    else if (highlighted->isFriendly())
      col = colors[ColorId::GREEN];
    drawHint(renderer, col, highlighted->getDescription(true));
  }
  if (optionsGui) {
    int margin = 10;
    optionsGui->setBounds(Rectangle(margin, getBounds().getKY() - optionsHeight - margin, 380, getBounds().getKY() - margin));
    optionsGui->render(renderer);
  }
}

void MapGui::resetHint() {
  hint.clear();
}

PGuiElem MapGui::getHintCallback(const string& s) {
  return GuiElem::mouseOverAction([this, s]() { hint = s; });
}

void MapGui::setOptions(const string& title, vector<PGuiElem> options) {
  int margin = 10;
  vector<PGuiElem> lines = makeVec<PGuiElem>(GuiElem::label(title, colors[ColorId::WHITE]));
  vector<int> heights {40};
  for (auto& elem : options) {
    lines.push_back(GuiElem::margins(std::move(elem), 15, 0, 0, 0));
    heights.push_back(30);
  }
  optionsHeight = 30 * options.size() + 40 + 2 * margin;
  optionsGui = GuiElem::stack(
      GuiElem::rectangle(transparency(colors[ColorId::BLACK], bgTransparency)),
      GuiElem::margins(GuiElem::verticalList(std::move(lines), heights, 0), margin, margin, margin, margin));
}

void MapGui::clearOptions() {
  optionsGui.reset();
}

