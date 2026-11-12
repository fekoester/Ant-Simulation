#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ants/colony.hpp"
#include "ants/render.hpp"

using namespace ants;

static Params small(uint64_t seed, int size = 40) {
    Params p;
    p.seed = seed;
    p.width = p.height = size;
    p.foodPerAnt = 60;
    return p;
}

TEST_CASE("food is conserved and the nest bookkeeping adds up") {
    for (uint64_t seed : {1, 2, 3}) {
        Params p = small(seed);
        Colony c(p);
        for (int s = 0; s < 3000; s++) {
            c.step();
            CHECK(c.initialFood == c.foodInWorld() + c.carried() + c.delivered);
            CHECK(c.nestFood == p.initialNestFood + c.delivered - (long)p.foodPerAnt * ((long)c.ants.size() - p.initialAnts));
            CHECK(c.nestFood >= 0);
        }
        CHECK(c.delivered > 0);
        CHECK((int)c.ants.size() > p.initialAnts);
    }
}

TEST_CASE("ants stay inside even small worlds, and carry no more than they can") {
    for (int size : {5, 7, 12}) {
        Params p = small(9, size);
        p.foodChance = 0.2;
        p.foodPerAnt = 20;
        p.carryMin = 20, p.carryMax = 20;
        Colony c(p);
        for (int s = 0; s < 2000; s++) {
            c.step();
            for (const auto &a : c.ants) {
                CHECK(c.inside(a.x, a.y));
                CHECK(a.carrying <= a.capacity);
                CHECK((a.carrying > 0) == (a.state == State::ToNest));
            }
        }
    }
}

TEST_CASE("the same seed gives the same run") {
    Colony a(small(5)), b(small(5)), d(small(6));
    for (int s = 0; s < 1500; s++) a.step(), b.step(), d.step();
    REQUIRE(a.ants.size() == b.ants.size());
    for (size_t i = 0; i < a.ants.size(); i++) {
        CHECK(a.ants[i].x == b.ants[i].x);
        CHECK(a.ants[i].y == b.ants[i].y);
    }
    CHECK(a.delivered == b.delivered);
    CHECK(a.pheromoneTotal() == b.pheromoneTotal());
    CHECK(a.delivered != d.delivered);
}

TEST_CASE("food detection is symmetric and picks the nearest pile") {
    Params p = small(1);
    p.foodChance = 0.0;
    Colony c(p);
    Ant a;
    a.x = 20, a.y = 20;
    c.at(17, 20).food = 100;  // 3 to the left: the original range stopped at -r .. r-1 in x and y alike
    c.detectFood(a);
    CHECK(a.state == State::ToFood);
    CHECK(a.targetX == 17);
    a.state = State::Search;
    c.at(23, 23).food = 100;
    c.at(21, 21).food = 100;  // nearer than both
    c.detectFood(a);
    CHECK(a.targetX == 21);
    CHECK(a.targetY == 21);
    a.state = State::Search;
    Ant edge;
    edge.x = 0, edge.y = 0;  // used to read outside the grid
    c.detectFood(edge);
    CHECK(edge.state == State::Search);
}

TEST_CASE("an ant that finds its pile emptied goes back to searching") {
    Params p = small(1);
    p.foodChance = 0.0;
    Colony c(p);
    c.ants.clear();
    Ant a;
    a.x = 10, a.y = 10, a.state = State::ToFood, a.targetX = 11, a.targetY = 11, a.capacity = 30;
    c.ants.push_back(a);
    c.step();
    CHECK(c.ants[0].state == State::Search);
    CHECK(c.ants[0].carrying == 0);
    // a pile smaller than the capacity: it takes what is there, no more
    c.at(13, 13).food = 7;
    c.ants[0].state = State::ToFood, c.ants[0].targetX = 13, c.ants[0].targetY = 13;
    for (int s = 0; s < 3; s++) c.step();
    CHECK(c.ants[0].carrying == 7);
    CHECK(c.at(13, 13).food == 0);
}

TEST_CASE("pheromone deposit kernel, including the map border") {
    Params p = small(1);
    p.foodChance = 0.0;
    Colony c(p);
    c.layPheromone(20, 20, 9.0);
    CHECK(c.at(20, 20).pheromone == doctest::Approx(9.0));
    CHECK(c.at(21, 20).pheromone == doctest::Approx(3.0));
    CHECK(c.at(21, 21).pheromone == doctest::Approx(3.0));
    CHECK(c.at(22, 21).pheromone == doctest::Approx(1.0));
    CHECK(c.at(23, 20).pheromone == 0.0);
    c.layPheromone(0, 0, 9.0);  // row and column 0 get pheromone too (the original skipped index 0)
    CHECK(c.at(0, 0).pheromone == doctest::Approx(9.0));
    CHECK(c.depositWeight(c.nestX + 4, c.nestY) == doctest::Approx(std::pow(1.25, 4)));
}

TEST_CASE("following picks tiles in proportion to their pheromone") {
    Params p = small(2);
    p.foodChance = 0.0;
    Colony c(p);
    c.at(21, 20).pheromone = 1.0;
    c.at(19, 20).pheromone = 3.0;
    int right = 0, left = 0;
    const int n = 20000;
    for (int i = 0; i < n; i++) {
        Ant a;
        a.x = 20, a.y = 20;
        REQUIRE(c.followPheromone(a));
        right += a.x == 21;
        left += a.x == 19;
    }
    CHECK(right + left == n);
    CHECK(double(left) / n == doctest::Approx(0.75).epsilon(0.03));
    Ant lonely;
    lonely.x = 5, lonely.y = 30;
    CHECK_FALSE(c.followPheromone(lonely));
}

TEST_CASE("searching ants alternate between following and random walk") {
    Params p = small(3);
    p.foodChance = 0.0;
    p.evaporation = 0.0;
    Colony c(p);
    for (auto &t : c.tiles) t.pheromone = 1.0;  // pheromone everywhere: without the break they would follow forever
    int breaks = 0, resumed = 0;
    bool inBreak = false;
    for (int s = 0; s < 400; s++) {
        c.step();
        const Ant &a = c.ants[0];
        if (a.randomLeft > 0 && !inBreak) breaks++, inBreak = true;
        if (a.randomLeft == 0 && inBreak) resumed++, inBreak = false;
    }
    CHECK(breaks >= 10);  // the original timers went negative and never switched again
    CHECK(resumed >= 9);
}

TEST_CASE("evaporation and the pheromone floor") {
    Params p = small(1);
    p.foodChance = 0.0;
    p.initialAnts = 0;
    Colony c(p);
    c.at(3, 3).pheromone = 1.0;
    c.step();
    CHECK(c.at(3, 3).pheromone == doctest::Approx(0.75));
    c.step(), c.step();  // 0.42 < 0.5
    CHECK(c.at(3, 3).pheromone == 0.0);
}

TEST_CASE("the ant cap and invalid parameters") {
    Params p = small(4);
    p.maxAnts = 5;
    p.initialNestFood = 100000;
    Colony c(p);
    for (int s = 0; s < 50; s++) c.step();
    CHECK(c.ants.size() == 5u);
    p.width = 3;
    CHECK_THROWS_AS(Colony{p}, std::invalid_argument);
}

TEST_CASE("rendering") {
    Colony c(small(1));
    for (int s = 0; s < 100; s++) c.step();
    Image img = render(c, 3);
    CHECK(img.w == 120);
    CHECK(img.rgb.size() == 120u * 120u * 3u);
}
