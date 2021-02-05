#!/usr/bin/env bash
# THIS_DIR=$( cd "$( dirname "$0" )" && pwd )
# cd $THIS_DIR && ./player --game-type=holdem  --card-abstraction cluster --card-abs-param ./result/buckets.all.holdem.13.4.3.re_neeo \
# --gamedef ./games/holdem.limit.2p.game  --init-strategy ./result/holdem.limit.2p.game_neeo.strategy.7 \
# --host $1 --port $2
THIS_DIR=$( cd "$( dirname "$0" )" && pwd )
cd $THIS_DIR && ./player --game-type=holdem  --card-abstraction cluster --card-abs-param ./result/is_cluster.card_abs \
--gamedef ./games/fhp.limit.2p.game  --init-strategy ./result/fhp.limit.2p.game.strategy.1 \
--host $1 --port $2