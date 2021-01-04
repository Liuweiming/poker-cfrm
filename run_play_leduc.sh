#!/usr/bin/env bash
THIS_DIR=$( cd "$( dirname "$0" )" && pwd )
cd $THIS_DIR && ./player  --game-type=leduc \
--gamedef ./games/leduc.limit.2p.game  --init-strategy /data/liuwm/cfrm/leduc.limit.2p.game.strategy.37 \
--host $1 --port $2