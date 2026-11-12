#!/usr/bin/env sh
# Foraging with and without pheromone following, several seeds. Usage: tools/experiment.sh [ants-binary] [steps]
set -e
BIN=${1:-./ants}
STEPS=${2:-5000}
mkdir -p results
for seed in 1 2 3 4 5; do
  "$BIN" --no-window --steps "$STEPS" --seed "$seed" --csv "results/pheromones_seed$seed.csv" --every 50
  "$BIN" --no-window --steps "$STEPS" --seed "$seed" --csv "results/random_walk_seed$seed.csv" --every 50 --no-pheromones
done
