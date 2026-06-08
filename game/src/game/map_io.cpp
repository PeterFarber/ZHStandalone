// MapDefinition JSON load/save (maps/*.json); merges file fields onto baked default layout.

#include "zh/game/map_io.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>

namespace zh::game {

namespace {

using json = nlohmann::json;

[[nodiscard]] char const *visual_kind_to_string(MapVisualKind kind) noexcept {
    switch (kind) {
        case MapVisualKind::IceTile:
            return "ice_tile";
        case MapVisualKind::WallFrame:
            return "wall_frame";
        case MapVisualKind::CircleMark:
            return "circle_mark";
        case MapVisualKind::GoalSprite:
            return "goal_sprite";
    }
    return "ice_tile";
}

[[nodiscard]] MapVisualKind visual_kind_from_string(std::string const &s) noexcept {
    if (s == "wall_frame") {
        return MapVisualKind::WallFrame;
    }
    if (s == "circle_mark") {
        return MapVisualKind::CircleMark;
    }
    if (s == "goal_sprite") {
        return MapVisualKind::GoalSprite;
    }
    return MapVisualKind::IceTile;
}

[[nodiscard]] char const *collider_kind_to_string(MapColliderKind kind) noexcept {
    switch (kind) {
        case MapColliderKind::Circle:
            return "circle";
        case MapColliderKind::Arc:
            return "arc";
        case MapColliderKind::Line:
            return "line";
        case MapColliderKind::Rect:
        default:
            return "rect";
    }
}

[[nodiscard]] MapColliderKind collider_kind_from_string(std::string const &s) noexcept {
    if (s == "circle") {
        return MapColliderKind::Circle;
    }
    if (s == "arc") {
        return MapColliderKind::Arc;
    }
    if (s == "line") {
        return MapColliderKind::Line;
    }
    return MapColliderKind::Rect;
}

[[nodiscard]] char const *shape_kind_to_string(MapShapeKind kind) noexcept {
    switch (kind) {
        case MapShapeKind::Circle:
            return "circle";
        case MapShapeKind::Line:
            return "line";
        case MapShapeKind::Arc:
            return "arc";
        case MapShapeKind::Rect:
        default:
            return "rect";
    }
}

[[nodiscard]] MapShapeKind shape_kind_from_string(std::string const &s) noexcept {
    if (s == "circle") {
        return MapShapeKind::Circle;
    }
    if (s == "line") {
        return MapShapeKind::Line;
    }
    if (s == "arc") {
        return MapShapeKind::Arc;
    }
    return MapShapeKind::Rect;
}

void write_rect(json &obj, char const *key, RectF const &r) {
    obj[key] = json{{"x", r.x}, {"y", r.y}, {"w", r.w}, {"h", r.h}};
}

void write_point(json &obj, char const *key, MapPoint const &p) {
    obj[key] = json{{"x", p.x}, {"y", p.y}};
}

void write_optional_point(json &obj, char const *key, std::optional<MapPoint> const &p) {
    if (p) {
        write_point(obj, key, *p);
    } else {
        obj[key] = nullptr;
    }
}

[[nodiscard]] RectF read_rect(json const &obj, char const *key, RectF fallback = {}) {
    if (!obj.contains(key) || !obj[key].is_object()) {
        return fallback;
    }
    json const &r = obj[key];
    return RectF{r.value("x", fallback.x), r.value("y", fallback.y), r.value("w", fallback.w),
                 r.value("h", fallback.h)};
}

[[nodiscard]] MapPoint read_point(json const &obj, char const *key, MapPoint fallback = {}) {
    if (!obj.contains(key) || !obj[key].is_object()) {
        return fallback;
    }
    json const &p = obj[key];
    return MapPoint{p.value("x", fallback.x), p.value("y", fallback.y)};
}

[[nodiscard]] std::optional<MapPoint> read_optional_point(json const &obj, char const *key) {
    if (!obj.contains(key) || obj[key].is_null()) {
        return std::nullopt;
    }
    return read_point(obj, key);
}

void read_team_spawns(json const &obj, MapTeamSpawns &out) {
    out.goalie = read_optional_point(obj, "goalie");
    out.skaters = {};
    if (obj.contains("skaters") && obj["skaters"].is_array()) {
        for (std::size_t i = 0; i < out.skaters.size() && i < obj["skaters"].size(); ++i) {
            if (obj["skaters"][i].is_null()) {
                out.skaters[i] = std::nullopt;
            } else {
                json const &sk = obj["skaters"][i];
                out.skaters[i] = MapPoint{sk.value("x", 0.f), sk.value("y", 0.f)};
            }
        }
    }
}

void write_team_spawns(json &obj, char const *key, MapTeamSpawns const &team) {
    json team_obj;
    write_optional_point(team_obj, "goalie", team.goalie);
    json skaters = json::array();
    for (std::optional<MapPoint> const &skater : team.skaters) {
        if (skater) {
            skaters.push_back({{"x", skater->x}, {"y", skater->y}});
        } else {
            skaters.push_back(nullptr);
        }
    }
    team_obj["skaters"] = std::move(skaters);
    obj[key] = std::move(team_obj);
}

[[nodiscard]] MapArc3 read_arc(json const &obj) {
    MapArc3 arc{};
    arc.p1 = read_point(obj, "p1");
    arc.p2 = read_point(obj, "p2");
    arc.p3 = read_point(obj, "p3");
    return arc;
}

void write_arc(json &obj, MapArc3 const &arc) {
    write_point(obj, "p1", arc.p1);
    write_point(obj, "p2", arc.p2);
    write_point(obj, "p3", arc.p3);
}

[[nodiscard]] std::optional<MapBackground> read_background(json const &root,
                                                         MapDefinition const &fallback) {
    if (!root.contains("background") || root["background"].is_null()) {
        return std::nullopt;
    }
    json const &b = root["background"];
    if (!b.is_object()) {
        return std::nullopt;
    }

    MapBackground bg{};
    if (b.contains("color") && b["color"].is_string()) {
        std::string const color = b["color"].get<std::string>();
        if (!color.empty()) {
            bg.color = color;
        }
    }
    if (b.contains("image") && b["image"].is_string()) {
        std::string const image = b["image"].get<std::string>();
        if (!image.empty()) {
            bg.image = image;
        }
    }
    if (!bg.color.has_value() && !bg.image.has_value()) {
        return std::nullopt;
    }

    bg.dest = read_rect(b, "dest", fallback.camera_bounds);
    if (b.contains("src") && b["src"].is_object()) {
        bg.src = read_rect(b, "src");
        bg.use_src = bg.src.w > 0.f && bg.src.h > 0.f;
    }
    bg.repeat = b.value("repeat", bg.image.has_value());
    return bg;
}

void write_background(json &root, std::optional<MapBackground> const &background) {
    if (!background.has_value()) {
        root["background"] = nullptr;
        return;
    }
    MapBackground const &bg = *background;
    json obj;
    if (bg.color.has_value()) {
        obj["color"] = *bg.color;
    }
    if (bg.image.has_value()) {
        obj["image"] = *bg.image;
    }
    write_rect(obj, "dest", bg.dest);
    if (bg.use_src) {
        write_rect(obj, "src", bg.src);
    }
    if (bg.repeat) {
        obj["repeat"] = true;
    }
    root["background"] = std::move(obj);
}

[[nodiscard]] std::optional<MapGoalZone> read_goal_zone(json const &root,
                                                        char const *key,
                                                        std::optional<MapGoalZone> fallback) {
    if (!root.contains(key) || root[key].is_null()) {
        return std::nullopt;
    }
    json const &v = root[key];
    if (!v.is_object()) {
        return fallback;
    }

    MapGoalZone zone{};
    if (v.contains("kind")) {
        std::string const kind = v.value("kind", std::string{"rect"});
        if (kind == "arc") {
            zone.kind = MapZoneShapeKind::Arc;
            zone.arc = read_arc(v);
        } else {
            zone.kind = MapZoneShapeKind::Rect;
            zone.rect = read_rect(v, "rect");
        }
        if (v.contains("tag") && v["tag"].is_string()) {
            zone.tag = v["tag"].get<std::string>();
        }
        return zone;
    }

    zone.kind = MapZoneShapeKind::Rect;
    zone.rect = RectF{v.value("x", 0.f), v.value("y", 0.f), v.value("w", 0.f), v.value("h", 0.f)};
    return zone;
}

void write_goal_zone(json &root, char const *key, std::optional<MapGoalZone> const &zone) {
    if (!zone.has_value()) {
        root[key] = nullptr;
        return;
    }
    json obj;
    if (zone->kind == MapZoneShapeKind::Arc) {
        obj["kind"] = "arc";
        write_arc(obj, zone->arc);
    } else {
        obj["kind"] = "rect";
        write_rect(obj, "rect", zone->rect);
    }
    if (zone->tag.has_value()) {
        obj["tag"] = *zone->tag;
    }
    root[key] = std::move(obj);
}

}  // namespace

std::string default_map_path() noexcept {
    return "maps/default.json";
}

bool load_map_file(char const *path, MapDefinition &out) noexcept {
    std::ifstream in{path};
    if (!in) {
        std::fprintf(stderr, "[zh] Map file not found: %s\n", path);
        return false;
    }

    json root;
    try {
        in >> root;
    } catch (json::parse_error const &e) {
        std::fprintf(stderr, "[zh] Map JSON parse error (%s): %s\n", path, e.what());
        return false;
    }

    // Start from baked default so missing JSON keys keep playable colliders/spawns/visuals.
    MapDefinition map = make_baked_default_map();
    map.version = root.value("version", kMapSchemaVersion);
    map.name = root.value("name", map.name);
    map.world_per_tex = root.value("world_per_tex", map.world_per_tex);

    map.camera_bounds = read_rect(root, "camera_bounds", map.camera_bounds);
    map.player_bounds = read_rect(root, "player_bounds", map.player_bounds);
    // Legacy playfield min/max object when player_bounds absent.
    if ((map.player_bounds.w <= 0.f || map.player_bounds.h <= 0.f) &&
        root.contains("playfield") && root["playfield"].is_object()) {
        json const &pf = root["playfield"];
        float const min_x = pf.value("min_x", map.player_bounds.x);
        float const max_x = pf.value("max_x", min_x);
        float const min_y = pf.value("min_y", map.player_bounds.y);
        float const max_y = pf.value("max_y", min_y);
        map.player_bounds = RectF{min_x, min_y, max_x - min_x, max_y - min_y};
    }
    map.goal_west = read_goal_zone(root, "goal_west", map.goal_west);
    map.goal_east = read_goal_zone(root, "goal_east", map.goal_east);
    map.background = read_background(root, map);

    if (root.contains("spawns") && root["spawns"].is_object()) {
        json const &sp = root["spawns"];
        map.spawns.puck_start = read_optional_point(sp, "puck_start");
        map.spawns.puck_faceoff = read_optional_point(sp, "puck_faceoff");
        map.spawns.host = read_optional_point(sp, "host");
        if (sp.contains("slots") && sp["slots"].is_array()) {
            for (std::size_t i = 0; i < map.spawns.slots.size() && i < sp["slots"].size(); ++i) {
                if (sp["slots"][i].is_null()) {
                    map.spawns.slots[i] = std::nullopt;
                } else {
                    json const &slot = sp["slots"][i];
                    map.spawns.slots[i] = MapPoint{slot.value("x", 0.f), slot.value("y", 0.f)};
                }
            }
        }
        if (sp.contains("team_a") && sp["team_a"].is_object()) {
            read_team_spawns(sp["team_a"], map.spawns.team_a);
        }
        if (sp.contains("team_b") && sp["team_b"].is_object()) {
            read_team_spawns(sp["team_b"], map.spawns.team_b);
        }
    }

    map.visuals.clear();
    if (root.contains("visuals") && root["visuals"].is_array()) {
        for (json const &v : root["visuals"]) {
            MapVisualPiece piece{};
            piece.id = v.value("id", std::string{});
            piece.kind = visual_kind_from_string(v.value("kind", std::string{"ice_tile"}));
            piece.dest = read_rect(v, "dest");
            piece.src = read_rect(v, "src");
            piece.use_src = v.value("use_src", false);
            piece.flip_x = v.value("flip_x", false);
            piece.slice_l = v.value("slice_l", 56.f);
            piece.slice_t = v.value("slice_t", 60.f);
            piece.slice_r = v.value("slice_r", 57.f);
            piece.slice_b = v.value("slice_b", 47.f);
            if (v.contains("z") && v["z"].is_number_integer()) {
                piece.z = v["z"].get<int>();
            }
            if (v.contains("tag") && v["tag"].is_string()) {
                piece.tag = v["tag"].get<std::string>();
            }
            map.visuals.push_back(std::move(piece));
        }
    }

    map.shapes.clear();
    if (root.contains("shapes") && root["shapes"].is_array()) {
        for (json const &s : root["shapes"]) {
            MapShape shape{};
            shape.kind = shape_kind_from_string(s.value("kind", std::string{"rect"}));
            if (shape.kind == MapShapeKind::Circle) {
                shape.x = s.value("x", 0.f);
                shape.y = s.value("y", 0.f);
                shape.radius = s.value("radius", 0.f);
            } else if (shape.kind == MapShapeKind::Line) {
                shape.line_p1 = read_point(s, "p1");
                shape.line_p2 = read_point(s, "p2");
            } else if (shape.kind == MapShapeKind::Arc) {
                shape.arc = read_arc(s);
            } else {
                shape.rect = read_rect(s, "rect");
                shape.corner_radius = s.value("cornerRadius", 0.f);
            }
            if (s.contains("fill") && s["fill"].is_string()) {
                std::string const fill = s["fill"].get<std::string>();
                if (!fill.empty()) {
                    shape.fill = fill;
                }
            }
            if (s.contains("stroke") && s["stroke"].is_string()) {
                std::string const stroke = s["stroke"].get<std::string>();
                if (!stroke.empty()) {
                    shape.stroke = stroke;
                }
            }
            shape.stroke_width = s.value("strokeWidth", 2.f);
            if (s.contains("z") && s["z"].is_number_integer()) {
                shape.z = s["z"].get<int>();
            }
            if (s.contains("tag") && s["tag"].is_string()) {
                shape.tag = s["tag"].get<std::string>();
            }
            map.shapes.push_back(std::move(shape));
        }
    }

    map.colliders.clear();
    if (root.contains("colliders") && root["colliders"].is_array()) {
        for (json const &c : root["colliders"]) {
            MapCollider col{};
            col.kind = collider_kind_from_string(c.value("kind", std::string{"rect"}));
            if (col.kind == MapColliderKind::Circle) {
                col.rect.x = c.value("x", 0.f);
                col.rect.y = c.value("y", 0.f);
                col.radius = c.value("radius", 0.f);
            } else if (col.kind == MapColliderKind::Arc) {
                col.arc = read_arc(c);
            } else if (col.kind == MapColliderKind::Line) {
                col.line_p1 = read_point(c, "p1");
                col.line_p2 = read_point(c, "p2");
            } else {
                col.rect = read_rect(c, "rect");
                col.corner_radius = c.value("cornerRadius", 0.f);
            }
            if (c.contains("tag") && c["tag"].is_string()) {
                col.tag = c["tag"].get<std::string>();
            }
            map.colliders.push_back(std::move(col));
        }
    }

    if (root.contains("playfield") && root["playfield"].is_object() && map.colliders.empty()) {
        json const &pf = root["playfield"];
        append_capsule_board_colliders(map.colliders, pf.value("min_x", 0.f), pf.value("max_x", 0.f),
                                       pf.value("min_y", 0.f), pf.value("max_y", 0.f));
    }

    out = std::move(map);
    std::printf("[zh] Loaded map '%s' from %s\n", out.name.c_str(), path);
    return true;
}

bool save_map_file(char const *path, MapDefinition const &def) noexcept {
    json root;
    root["version"] = def.version;
    root["name"] = def.name;
    root["world_per_tex"] = def.world_per_tex;
    write_rect(root, "camera_bounds", def.camera_bounds);
    write_rect(root, "player_bounds", def.player_bounds);
    write_goal_zone(root, "goal_west", def.goal_west);
    write_goal_zone(root, "goal_east", def.goal_east);
    write_background(root, def.background);

    json spawns;
    if (def.spawns.puck_start) {
        write_point(spawns, "puck_start", *def.spawns.puck_start);
    } else {
        spawns["puck_start"] = nullptr;
    }
    if (def.spawns.puck_faceoff) {
        write_point(spawns, "puck_faceoff", *def.spawns.puck_faceoff);
    } else {
        spawns["puck_faceoff"] = nullptr;
    }
    if (def.spawns.host) {
        write_point(spawns, "host", *def.spawns.host);
    } else {
        spawns["host"] = nullptr;
    }
    json slots = json::array();
    for (std::optional<MapPoint> const &slot : def.spawns.slots) {
        if (slot) {
            slots.push_back({{"x", slot->x}, {"y", slot->y}});
        } else {
            slots.push_back(nullptr);
        }
    }
    spawns["slots"] = std::move(slots);
    write_team_spawns(spawns, "team_a", def.spawns.team_a);
    write_team_spawns(spawns, "team_b", def.spawns.team_b);
    root["spawns"] = std::move(spawns);

    json visuals = json::array();
    for (MapVisualPiece const &piece : def.visuals) {
        json v;
        v["id"] = piece.id;
        v["kind"] = visual_kind_to_string(piece.kind);
        write_rect(v, "dest", piece.dest);
        if (piece.use_src) {
            write_rect(v, "src", piece.src);
            v["use_src"] = true;
        }
        if (piece.flip_x) {
            v["flip_x"] = true;
        }
        if (piece.kind == MapVisualKind::WallFrame) {
            v["slice_l"] = piece.slice_l;
            v["slice_t"] = piece.slice_t;
            v["slice_r"] = piece.slice_r;
            v["slice_b"] = piece.slice_b;
        }
        if (piece.z.has_value()) {
            v["z"] = *piece.z;
        }
        if (piece.tag.has_value()) {
            v["tag"] = *piece.tag;
        }
        visuals.push_back(std::move(v));
    }
    root["visuals"] = std::move(visuals);

    json shapes = json::array();
    for (MapShape const &shape : def.shapes) {
        json s;
        s["kind"] = shape_kind_to_string(shape.kind);
        if (shape.kind == MapShapeKind::Circle) {
            s["x"] = shape.x;
            s["y"] = shape.y;
            s["radius"] = shape.radius;
        } else if (shape.kind == MapShapeKind::Line) {
            write_point(s, "p1", shape.line_p1);
            write_point(s, "p2", shape.line_p2);
        } else if (shape.kind == MapShapeKind::Arc) {
            write_arc(s, shape.arc);
        } else {
            write_rect(s, "rect", shape.rect);
            if (shape.corner_radius > 1e-4f) {
                s["cornerRadius"] = shape.corner_radius;
            }
        }
        if (shape.fill.has_value()) {
            s["fill"] = *shape.fill;
        }
        if (shape.stroke.has_value()) {
            s["stroke"] = *shape.stroke;
        }
        if (shape.stroke_width != 2.f) {
            s["strokeWidth"] = shape.stroke_width;
        }
        if (shape.z.has_value()) {
            s["z"] = *shape.z;
        }
        if (shape.tag.has_value()) {
            s["tag"] = *shape.tag;
        }
        shapes.push_back(std::move(s));
    }
    root["shapes"] = std::move(shapes);

    json colliders = json::array();
    for (MapCollider const &col : def.colliders) {
        json c;
        c["kind"] = collider_kind_to_string(col.kind);
        if (col.kind == MapColliderKind::Circle) {
            c["x"] = col.rect.x;
            c["y"] = col.rect.y;
            c["radius"] = col.radius;
        } else if (col.kind == MapColliderKind::Arc) {
            write_arc(c, col.arc);
        } else if (col.kind == MapColliderKind::Line) {
            write_point(c, "p1", col.line_p1);
            write_point(c, "p2", col.line_p2);
        } else {
            write_rect(c, "rect", col.rect);
            if (col.corner_radius > 1e-4f) {
                c["cornerRadius"] = col.corner_radius;
            }
        }
        if (col.tag.has_value()) {
            c["tag"] = *col.tag;
        }
        colliders.push_back(std::move(c));
    }
    root["colliders"] = std::move(colliders);

    std::ofstream out{path, std::ios::trunc};
    if (!out) {
        std::fprintf(stderr, "[zh] Failed to write map: %s\n", path);
        return false;
    }
    out << root.dump(2) << '\n';
    return static_cast<bool>(out);
}

}  // namespace zh::game
