// Ant-Simulation: watch the colony in an SFML window or run it headless.
//
//   ants [--seed N] [--size WxH] [--ants N] [--no-pheromones] [--evaporation 0.25] [--max-ants N]
//        [--steps N --no-window] [--csv stats.csv --every 50] [--dump-dir DIR --dump-every 20 --scale 4]
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "ants/colony.hpp"
#include "ants/render.hpp"

#ifndef ANTS_NO_SFML
#include <SFML/Graphics.hpp>
#endif

using namespace ants;

namespace {

struct Options {
    Params params;
    long steps = -1, every = 50, dumpEvery = 20;
    std::string csv, dumpDir;
    int scale = 4;
    bool window = true;
};

void usage() {
    std::cout << "usage: ants [--seed N] [--size WxH] [--ants N] [--no-pheromones] [--evaporation E]\n"
                 "            [--food-per-ant N] [--max-ants N] [--steps N] [--no-window]\n"
                 "            [--csv stats.csv] [--every 50] [--dump-dir DIR] [--dump-every 20] [--scale 4]\n"
                 "viewer: space pause, up/down speed, mouse wheel zoom, right drag pan, R reset view,\n"
                 "        P toggle pheromone following, N new world, Esc quit\n";
}

void csvHeader(std::ostream &f) {
    f << "step,ants,delivered,nest_food,food_in_world,carried,pheromone_total,searching,to_food,to_nest\n";
}
void csvRow(std::ostream &f, const Colony &c) {
    f << c.steps << ',' << c.ants.size() << ',' << c.delivered << ',' << c.nestFood << ',' << c.foodInWorld() << ','
      << c.carried() << ',' << c.pheromoneTotal() << ',' << c.countState(State::Search) << ','
      << c.countState(State::ToFood) << ',' << c.countState(State::ToNest) << '\n';
}

#ifndef ANTS_NO_SFML
#if SFML_VERSION_MAJOR >= 3
#define KEY(k) sf::Keyboard::Key::k
#else
#define KEY(k) sf::Keyboard::k
#endif

void view(const Options &opt) {
    auto colony = std::make_unique<Colony>(opt.params);
    const unsigned W = (unsigned)colony->W, H = (unsigned)colony->H;
    const float scale = std::max(1.0f, std::min(900.0f / W, 900.0f / H));
    const unsigned winW = (unsigned)(W * scale), winH = (unsigned)(H * scale);
#if SFML_VERSION_MAJOR >= 3
    sf::RenderWindow window(sf::VideoMode({winW, winH}), "Ant Simulation");
    sf::Texture tex;
    (void)tex.resize({W, H});
#else
    sf::RenderWindow window(sf::VideoMode(winW, winH), "Ant Simulation");
    sf::Texture tex;
    tex.create(W, H);
#endif
    window.setFramerateLimit(60);
    sf::View camera = window.getDefaultView();
    bool paused = false, dragging = false;
    int stepsPerFrame = 1;
    sf::Vector2i lastMouse;
    std::vector<uint8_t> rgba((size_t)W * H * 4, 255);

    auto title = [&]() {
        return "Ant Simulation - seed " + std::to_string(colony->p.seed) + " - step " + std::to_string(colony->steps) +
               " - ants " + std::to_string(colony->ants.size()) + " - delivered " +
               std::to_string(colony->delivered) + (colony->p.usePheromones ? "" : " - NO PHEROMONES") +
               (paused ? " - PAUSED" : " - x" + std::to_string(stepsPerFrame));
    };
    auto onKey = [&](auto code) {
        if (code == KEY(Escape)) window.close();
        else if (code == KEY(Space)) paused = !paused;
        else if (code == KEY(Up)) stepsPerFrame = std::min(256, stepsPerFrame * 2);
        else if (code == KEY(Down)) stepsPerFrame = std::max(1, stepsPerFrame / 2);
        else if (code == KEY(R)) camera = window.getDefaultView();
        else if (code == KEY(P)) colony->p.usePheromones = !colony->p.usePheromones;
        else if (code == KEY(N)) {
            Params p = colony->p;
            p.seed = std::random_device{}();
            colony = std::make_unique<Colony>(p);
        }
    };
    auto zoom = [&](float delta) { camera.zoom(delta > 0 ? 0.85f : 1.0f / 0.85f); };
    auto pan = [&](sf::Vector2i now) {
        const sf::Vector2f a = window.mapPixelToCoords(lastMouse, camera), b = window.mapPixelToCoords(now, camera);
        camera.move(a - b);
        lastMouse = now;
    };

    while (window.isOpen()) {
#if SFML_VERSION_MAJOR >= 3
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            else if (const auto *k = event->getIf<sf::Event::KeyPressed>()) onKey(k->code);
            else if (const auto *w = event->getIf<sf::Event::MouseWheelScrolled>()) zoom(w->delta);
            else if (const auto *m = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Right) dragging = true, lastMouse = m->position;
            } else if (const auto *m = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (m->button == sf::Mouse::Button::Right) dragging = false;
            } else if (const auto *m = event->getIf<sf::Event::MouseMoved>()) {
                if (dragging) pan(m->position);
            }
        }
#else
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            else if (event.type == sf::Event::KeyPressed) onKey(event.key.code);
            else if (event.type == sf::Event::MouseWheelScrolled) zoom(event.mouseWheelScroll.delta);
            else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right)
                dragging = true, lastMouse = {event.mouseButton.x, event.mouseButton.y};
            else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right)
                dragging = false;
            else if (event.type == sf::Event::MouseMoved && dragging)
                pan({event.mouseMove.x, event.mouseMove.y});
        }
#endif
        if (!window.isOpen()) break;
        if (!paused)
            for (int i = 0; i < stepsPerFrame; i++) colony->step();
        const Image img = render(*colony, 1, true);
        for (size_t i = 0; i < (size_t)W * H; i++)
            for (int k = 0; k < 3; k++) rgba[i * 4 + k] = img.rgb[i * 3 + k];
        tex.update(rgba.data());
        window.setTitle(title());
        window.clear();
        window.setView(camera);
        sf::Sprite map(tex);
#if SFML_VERSION_MAJOR >= 3
        map.setScale({scale, scale});
#else
        map.setScale(scale, scale);
#endif
        window.draw(map);
        window.display();
    }
}
#endif

}  // namespace

int main(int argc, char **argv) {
    Options opt;
    try {
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            auto next = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument(a + " needs a value");
                return argv[++i];
            };
            if (a == "-h" || a == "--help") return usage(), 0;
            else if (a == "--seed") opt.params.seed = std::stoull(next());
            else if (a == "--size") {
                const std::string s = next();
                const auto x = s.find('x');
                if (x == std::string::npos) throw std::invalid_argument("--size expects WxH");
                opt.params.width = std::stoi(s.substr(0, x)), opt.params.height = std::stoi(s.substr(x + 1));
            } else if (a == "--ants") opt.params.initialAnts = std::stoi(next());
            else if (a == "--no-pheromones") opt.params.usePheromones = false;
            else if (a == "--evaporation") opt.params.evaporation = std::stod(next());
            else if (a == "--food-per-ant") opt.params.foodPerAnt = std::stoi(next());
            else if (a == "--max-ants") opt.params.maxAnts = std::stoi(next());
            else if (a == "--steps") opt.steps = std::stol(next());
            else if (a == "--no-window") opt.window = false;
            else if (a == "--csv") opt.csv = next();
            else if (a == "--every") opt.every = std::max(1L, std::stol(next()));
            else if (a == "--dump-dir") opt.dumpDir = next();
            else if (a == "--dump-every") opt.dumpEvery = std::max(1L, std::stol(next()));
            else if (a == "--scale") opt.scale = std::max(1, std::stoi(next()));
            else throw std::invalid_argument("unknown argument " + a);
        }
        opt.params.validate();
        if (opt.window && opt.steps < 0) {
#ifdef ANTS_NO_SFML
            std::cerr << "built without SFML: use --no-window --steps N\n";
            return 1;
#else
            view(opt);
            return 0;
#endif
        }
        if (opt.steps < 0) opt.steps = 5000;
        Colony colony(opt.params);
        std::ofstream csv;
        if (!opt.csv.empty()) {
            csv.open(opt.csv);
            if (!csv) throw std::runtime_error("cannot write " + opt.csv);
            csvHeader(csv);
            csvRow(csv, colony);
        }
        if (!opt.dumpDir.empty()) std::filesystem::create_directories(opt.dumpDir);
        int frame = 0;
        for (long s = 1; s <= opt.steps; s++) {
            colony.step();
            if (csv && s % opt.every == 0) csvRow(csv, colony);
            if (!opt.dumpDir.empty() && s % opt.dumpEvery == 0) {
                char name[64];
                std::snprintf(name, sizeof name, "/frame_%05d.ppm", frame++);
                writePpm(opt.dumpDir + name, render(colony, opt.scale));
            }
        }
        std::printf("seed %llu, %ld steps: %zu ants, %ld food delivered (%ld left in the world)\n",
                    (unsigned long long)colony.p.seed, colony.steps, colony.ants.size(), colony.delivered,
                    colony.foodInWorld());
    } catch (const std::exception &e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        usage();
        return 2;
    }
    return 0;
}
