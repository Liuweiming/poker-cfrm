#!/usr/bin/env bash

./cluster-abs  --save-to ./neeo_cluster_5000.card_abs --threads 90 --dump-centers true --metric mixed_neeo --buckets 169,5000,5000,5000 --history-points 10,10,10,10 --nb-hist-samples-per-round 100,2652,52,2652
