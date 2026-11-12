// Ant colony foraging with distance-weighted pheromones (no graphics dependency).
//
// Ants leave the nest and search. They random-walk, or, when pheromone is around, step to one of the
// 9 surrounding tiles (staying put included) with probability proportional to its pheromone. When food is
// within detection range they walk to the nearest pile, take what they can carry and walk home. On both
// legs they lay pheromone, weighted by base^(distance to nest): more pheromone further from the nest, so
// following the concentration leads outward along trails, towards food. Pheromone evaporates every step.
// The nest makes a new ant for every foodPerAnt units delivered.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace ants {

struct Params {
    int width = 160, height = 160;
    uint64_t seed = 1;
    int initialAnts = 2;
    int initialNestFood = 0;
    int foodPerAnt = 150;          // nest food needed for a new ant
    int maxAnts = 0;               // 0 = no limit
    double evaporation = 0.25;     // fraction of pheromone lost per step
    double pheromoneFloor = 0.5;   // below this a tile's pheromone is set to 0
    double foodChance = 0.03;      // chance that a tile starts with a food pile
    int foodMin = 500, foodMax = 1000;
    int carryMin = 20, carryMax = 33;  // each ant's carrying capacity
    int detectionRange = 3;        // food is seen within this Chebyshev distance
    double depositToFood = 50.0;   // pheromone laid per step while walking to food (times base^distance)
    double depositToNest = 10.0;   // ... and while carrying food home
    double distanceBase = 1.25;
    int followMin = 4, followMax = 8;  // steps of pheromone following before a random-walk break
    int randomMin = 3, randomMax = 8;  // length of that break
    bool usePheromones = true;     // false: searching ants only random-walk (the control experiment)

    void validate() const {
        if (width < 5 || height < 5) throw std::invalid_argument("the world must be at least 5 x 5 tiles");
        if (foodMin > foodMax || carryMin > carryMax || followMin > followMax || randomMin > randomMax)
            throw std::invalid_argument("a minimum is larger than its maximum");
        if (carryMin < 1 || foodPerAnt < 1 || detectionRange < 0) throw std::invalid_argument("invalid ant parameters");
        if (evaporation < 0 || evaporation > 1) throw std::invalid_argument("evaporation must be in [0, 1]");
    }
};

enum class State : uint8_t { Search, ToFood, ToNest };

struct Ant {
    int x = 0, y = 0;
    State state = State::Search;
    int capacity = 20, carrying = 0;
    int targetX = 0, targetY = 0;
    int followLeft = 0, randomLeft = 0;
    long trips = 0;
};

struct Tile {
    int food = 0;
    double pheromone = 0.0;
};

class Colony {
public:
    explicit Colony(Params params) : p(std::move(params)), rng(p.seed) {
        p.validate();
        W = p.width, H = p.height;
        nestX = W / 2, nestY = H / 2;
        tiles.assign((size_t)W * H, Tile{});
        for (auto &t : tiles)
            if (uniform() < p.foodChance) t.food = dice(p.foodMin, p.foodMax);
        at(nestX, nestY).food = 0;
        for (const auto &t : tiles) initialFood += t.food;
        nestFood = p.initialNestFood;
        for (int i = 0; i < p.initialAnts; i++) spawnAnt();
    }

    void step() {
        for (size_t i = 0; i < ants.size(); i++) {
            Ant &a = ants[i];
            switch (a.state) {
                case State::Search:
                    move(a);
                    detectFood(a);
                    break;
                case State::ToFood: walkToFood(a); break;
                case State::ToNest: walkToNest(a); break;
            }
        }
        if (nestFood >= p.foodPerAnt && (p.maxAnts <= 0 || (int)ants.size() < p.maxAnts)) {
            nestFood -= p.foodPerAnt;
            spawnAnt();
        }
        for (auto &t : tiles) {
            t.pheromone *= 1.0 - p.evaporation;
            if (t.pheromone < p.pheromoneFloor) t.pheromone = 0.0;
        }
        steps++;
    }

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
    Tile &at(int x, int y) { return tiles[(size_t)y * W + x]; }
    const Tile &at(int x, int y) const { return tiles[(size_t)y * W + x]; }

    // pheromone laid on the centre tile; 1/3 of it within Manhattan distance 2, 1/9 on the rest of the 5x5 block
    void layPheromone(int x, int y, double amount) {
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                if (!inside(x + dx, y + dy)) continue;
                const int m = std::abs(dx) + std::abs(dy);
                at(x + dx, y + dy).pheromone += m == 0 ? amount : m <= 2 ? amount / 3.0 : amount / 9.0;
            }
    }
    double depositWeight(int x, int y) const {
        return std::pow(p.distanceBase, std::hypot(double(x - nestX), double(y - nestY)));
    }

    long foodInWorld() const {
        long s = 0;
        for (const auto &t : tiles) s += t.food;
        return s;
    }
    long carried() const {
        long s = 0;
        for (const auto &a : ants) s += a.carrying;
        return s;
    }
    double pheromoneTotal() const {
        double s = 0;
        for (const auto &t : tiles) s += t.pheromone;
        return s;
    }
    int countState(State s) const {
        return (int)std::count_if(ants.begin(), ants.end(), [&](const Ant &a) { return a.state == s; });
    }

    Params p;
    std::mt19937_64 rng;
    int W = 0, H = 0, nestX = 0, nestY = 0;
    std::vector<Tile> tiles;
    std::vector<Ant> ants;
    long nestFood = 0;      // food in the nest not yet spent on ants
    long delivered = 0;     // all food ever brought home
    long initialFood = 0;
    long steps = 0;

    // ---- behaviour (public for tests) ----
    int dice(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
    double uniform() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); }

    void spawnAnt() {
        Ant a;
        a.x = nestX, a.y = nestY;
        a.capacity = dice(p.carryMin, p.carryMax);
        a.followLeft = dice(p.followMin, p.followMax);
        ants.push_back(a);
    }

    void stepTowards(Ant &a, int tx, int ty) {
        a.x += (tx > a.x) - (tx < a.x);
        a.y += (ty > a.y) - (ty < a.y);
    }

    // follow pheromone for followLeft steps, then random-walk for a while (so ants do not get trapped on a trail)
    void move(Ant &a) {
        if (a.randomLeft > 0) {
            a.randomLeft--;
            randomWalk(a);
            return;
        }
        if (p.usePheromones && followPheromone(a)) {
            if (--a.followLeft <= 0) {
                a.randomLeft = dice(p.randomMin, p.randomMax);
                a.followLeft = dice(p.followMin, p.followMax);
            }
            return;
        }
        randomWalk(a);
    }

    // roulette over the 3x3 neighbourhood (inside the world) weighted by pheromone; false if there is none
    bool followPheromone(Ant &a) {
        double total = 0;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (inside(a.x + dx, a.y + dy)) total += at(a.x + dx, a.y + dy).pheromone;
        if (total <= 0) return false;
        double r = uniform() * total;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!inside(a.x + dx, a.y + dy)) continue;
                const double ph = at(a.x + dx, a.y + dy).pheromone;
                if (ph <= 0) continue;
                r -= ph;
                if (r <= 0) {
                    a.x += dx, a.y += dy;
                    return true;
                }
            }
        return true;  // rounding: stay
    }

    // x and y each move -1, 0 or +1 with equal probability; moves off the map are dropped
    void randomWalk(Ant &a) {
        const int dx = dice(-1, 1), dy = dice(-1, 1);
        if (inside(a.x + dx, a.y)) a.x += dx;
        if (inside(a.x, a.y + dy)) a.y += dy;
    }

    // nearest food pile (Euclidean) within detectionRange
    void detectFood(Ant &a) {
        const int r = p.detectionRange;
        long best = -1;
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (!inside(a.x + dx, a.y + dy) || at(a.x + dx, a.y + dy).food <= 0) continue;
                const long d2 = (long)dx * dx + (long)dy * dy;
                if (best < 0 || d2 < best) best = d2, a.targetX = a.x + dx, a.targetY = a.y + dy;
            }
        if (best >= 0) a.state = State::ToFood;
    }

    void walkToFood(Ant &a) {
        layPheromone(a.x, a.y, p.depositToFood * depositWeight(a.x, a.y));
        stepTowards(a, a.targetX, a.targetY);
        if (a.x != a.targetX || a.y != a.targetY) return;
        Tile &t = at(a.x, a.y);
        if (t.food <= 0) {  // another ant emptied it first
            a.state = State::Search;
            return;
        }
        a.carrying = std::min(a.capacity, t.food);
        t.food -= a.carrying;
        a.state = State::ToNest;
    }

    void walkToNest(Ant &a) {
        layPheromone(a.x, a.y, p.depositToNest * depositWeight(a.x, a.y));
        stepTowards(a, nestX, nestY);
        if (a.x != nestX || a.y != nestY) return;
        nestFood += a.carrying;
        delivered += a.carrying;
        a.carrying = 0;
        a.trips++;
        a.state = State::Search;
    }
};

}  // namespace ants
