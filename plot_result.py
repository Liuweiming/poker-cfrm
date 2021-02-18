import numpy as np
import re
import json
import os
import fnmatch
import matplotlib as mpl
mpl.use('Agg')
from matplotlib import pyplot as plt
import matplotlib as mpl
from cycler import cycler

decoder = json.JSONDecoder()
cmap = plt.get_cmap('tab20')
colors = list(cmap(i) for i in range(20))
hexcolor = list(map(lambda rgb: '#%02x%02x%02x' %
                    (int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255)), colors))
mpl.rcParams['axes.prop_cycle'] = cycler(color=hexcolor)


def moving_average(a, n=10):
    ret = np.cumsum(a, dtype=float)
    ret[n:] = (ret[n:] - ret[:-n]) / n
    ret[:n] /= np.arange(1, n + 1)
    return ret


label_name = []
plot_exp = True
plot_regret = True
plot_loss = False
save_data = False
length = 500000
plot_index = 0

pre_files = []

data = {}
pindex = 0

exp_fig, exp_axs = plt.subplots(1, 1)
exp_fig.set_size_inches(w=7, h=4)
exp_fig_name = ''
# true_leduc_cfr_exp = np.loadtxt("true_leduc_cfr_exp.out")
# true_leduc_cfr_exp = true_leduc_cfr_exp[1:101]
# exp_fig_name = '_deep_cfr_leduc_train_100_256_actor_4_cpu'

log_dir = "./results"
bash_dir = os.path.join(log_dir, "batch_run")
# result_dir = os.path.join(log_dir, "results-01-08")
result_dir = "./results"
fig_name = ""
base_comm = ""


for file, name in ([
    # ("18_02_2021.11_16_45", "FHP_2_5_null_abs_escfr"),
    # ("18_02_2021.11_16_30", "FHP_2_5_no_abs_escfr"),
    ("18_02_2021.11_18_40", "FHP_2_13_null_abs_escfr"),
    ("18_02_2021.11_19_08", "FHP_2_13_no_abs_escfr"),
    ("18_02_2021.13_19_21", "FHP_2_13_neeo_abs_escfr"),
] + pre_files):
    exploit = []
    localbr = []
    localbrstd = []
    num_step = []
    node_touched = []
    alpha = []
    num_trained = []
    num_sampled = []
    time_relative = []
    value = []
    regrets = {}
    strategies = {}
    N = {}
    s_loss = {}
    file = os.path.join(result_dir, file, 'debug.jsonl')
    if 'jsonl' in file:
        # print(f_n)
        with open(file) as fp:
            line = fp.readline()
            while line:
                res = re.search('Exploitability', line)
                if res:
                    exps = decoder.decode(line)
                    exploit.append(float(exps['Exploitability']))
                    num_step.append(int(exps['Step']))
                    try:
                        info_sets = exps['info_set']
                        for inf, info_values in info_sets.items():
                            actions = info_values['action']
                            actions = [str(a) for a in actions]
                            regret_net = info_values['regret_net']
                            policy_net = info_values['policy_net']
                            if inf not in regrets:
                                regrets[inf] = {a: [] for a in actions}
                            if inf not in strategies:
                                strategies[inf] = {a: [] for a in actions}
                            for a, v in zip(actions, regret_net):
                                if a not in regrets[inf]:
                                    regrets[inf][a] = []
                                regrets[inf][a].append(v)
                            for a, v in zip(actions, policy_net):
                                if a not in strategies[inf]:
                                    strategies[inf][a] = []
                                strategies[inf][a].append(v)
                    except:
                        pass

                line = fp.readline()
                if len(exploit) > length:
                    break
        if save_data:
            if name not in data:
                data[name] = {}
            data[name]['exploit'] = exploit
            data[name]['num_trained'] = num_trained
            data[name]['node_touched'] = node_touched
            data[name]['alpha'] = alpha

        if pindex == 0:
            exp_fig_name = fig_name + name + ".png"
        color_i = colors[pindex]
        if plot_exp:
            print(name, ":", exploit[-1])
            # exploit = moving_average(exploit, n=100)
            exp_axs.loglog(num_step[0:length],
                                 exploit[0:length], label=name, color=color_i)
            exp_axs.set_ylabel('exploitability')
            exp_axs.set_xlabel('steps')
            # exp_axs.set_xlim([100, num_step[-1]])
            exp_axs.legend(prop={'size': 5})
            exp_fig.savefig('results/images/exp_' + exp_fig_name)

        if plot_regret and pindex == plot_index:
            ax_num = len(regrets)
            # w = h = int(np.floor(np.sqrt(ax_num)))
            w = 4
            h = int(np.ceil(ax_num / w))
            h = max(2, min(h, 20))
            ax_num = w * h
            fig, axs = plt.subplots(h, w)
            fig.set_size_inches(4 * w, 4 * h)
            i = 0
            keys = list(regrets.keys())
            keys = sorted(keys)
            for inf in keys:
                reg = regrets[inf]
                if i >= ax_num:
                    break
                ax = axs[i // w][i % w]
                for a, v in reg.items():
                    if 'true' in a:
                        ax.plot(np.array(v[0:length]), '--', label=a)
                    else:
                        # v = np.array(v[0:length]) / /
                        #     np.sqrt(1 + np.arange(len(v[0:length])))
                        # print(v)
                        # v = np.maximum(v, -10)
                        # v = np.minimum(v, 10)
                        ax.plot(v[0:length], label=a)
                v = reg[list(reg.keys())[0]]
                ax.plot([0 for _ in range(len(v))][0:length], '--k')
                ax.set_title(inf)
                ax.legend()
                i += 1
            fig.savefig('results/images/regret_' + exp_fig_name)
            fig, axs = plt.subplots(h, w)
            fig.set_size_inches(4 * w, 4 * h)

            for i, inf in enumerate(keys):
                if i >= ax_num:
                    break
                reg = strategies[inf]
                ax = axs[i // w][i % w]
                for a, v in reg.items():
                    if 'true' in a:
                        ax.plot(v[0:length], '--', label=a)
                    else:
                        ax.plot(v[0:length], label=a)
                v = reg[list(reg.keys())[0]]
                ax.plot([0 for _ in range(len(v))][0:length], '--k')
                ax.plot([1 for _ in range(len(v))][0:length], '--k')
                ax.set_title(inf)
                ax.legend()
            plt.savefig('results/images/strategy_' + exp_fig_name)

        if plot_loss:
            plt.figure(100)
            # for loss_name, loss_v in s_loss.items():
            #     plt.semilogy(loss_v[0:length * 10], label=loss_name + ': ' + name + ': ' + file[4:])
            loss = np.sum(np.array([l for l in s_loss.values()]), axis=0)[
                0:length]
            plt.semilogy(loss, label=name)
            plt.title('losses')
            # plt.legend(prop={'size': 5})
            plt.legend()
            plt.savefig('results/images/losses_' + exp_fig_name + name)

    pindex += 1

if save_data:
    with open("paper/" + prefix + "_data.json", 'w') as fp:
        json.dump(data, fp)
