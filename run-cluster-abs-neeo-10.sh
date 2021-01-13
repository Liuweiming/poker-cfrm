#!/usr/bin/env bash

./cluster-abs  --save-to ./neeo_cluster_10.card_abs --threads 90 --dump-centers true --metric mixed_neeo --buckets 169,1,1,10 --history-points 1,2,2,8 --nb-hist-samples-per-round 100,2652,52,2652
