#!/usr/bin/env bash

./cluster-abs  --save-to ./neeo_cluster_large.card_abs --threads 90 --dump-centers true --metric mixed_neeo --buckets 169,10000,50000,5000 --history-points 50,50,50,8 --nb-hist-samples-per-round 100,2652,52,2652
