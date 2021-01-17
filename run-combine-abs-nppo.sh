#!/usr/bin/env bash

./potential-abs -p 3 --save-to ./nppo_cluster_5000_5000_5000.card_abs --potential-from neeo_cluster_1.card_abs --load-from /data/liuwm/cfrm/poker-cfrm/nppo_cluster_5000.card_abs --threads 90 --nb-buckets 5000 --handranks=./handranks.dat
