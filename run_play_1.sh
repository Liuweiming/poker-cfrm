#!/usr/bin/env bash
THIS_DIR=$( cd "$( dirname "$0" )" && pwd )
cd $THIS_DIR && ./player --game-type=holdem  --card-abstraction cluster --card-abs-param ./neeo_cluster.card_abs \
--gamedef ./games/holdem.limit.2p.game  --init-strategy /data/liuwm/cfrm/holdem.limit.2p.game.strategy.10 \
--host $1 --port $2