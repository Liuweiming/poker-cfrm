#!/usr/bin/env bash
THIS_DIR=$( cd "$( dirname "$0" )" && pwd )
cd $THIS_DIR && ./player --game-type=holdem  --card-abstraction cluster --card-abs-param /data/liuwm/poker-cfrm/poker-cfrm-1/nsss_cluster_large.card_abs \
--gamedef ./games/holdem.limit.2p.game  --init-strategy /data/liuwm/cfrm/holdem.limit.2p.game.nsss.strategy.1 \
--host $1 --port $2