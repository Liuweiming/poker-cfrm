#!/usr/bin/env bash
THIS_DIR=$( cd "$( dirname "$0" )" && pwd )
cd $THIS_DIR && ./player --game-type=holdem  --card-abstraction cluster --card-abs-param ./result/buckets.all.holdem.13.4.3.re_neeo \
--gamedef ./games/fhp.limit.2p.game  --init-strategy ./result/fhp.limit.2p.game.neeo.stategy.10 \
--host $1 --port $2