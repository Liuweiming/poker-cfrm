#!/usr/bin/env bash

./cluster-abs  --save-to ./nsss_cluster_100000.card_abs --threads 90 --dump-centers true --metric mixed_nsss --buckets 169,100000,100000,100000 --history-points 10,10,10,10 --nb-hist-samples-per-round 100,2652,52,2652
