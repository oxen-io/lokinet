#!/usr/bin/env python
#
# this script generate supervisord configs for running a test network on loopback
#


from argparse import ArgumentParser as AP
from configparser import ConfigParser as CP

import os


def svcNodeName(id): return 'svc-node-%03d' % id


def clientNodeName(id): return 'client-node-%03d' % id


def main():
    ap = AP()
    ap.add_argument('--valgrind', type=bool, default=False)
    ap.add_argument('--dir', type=str, default='testnet_tmp')
    ap.add_argument('--svc', type=int, default=20,
                    help='number of service nodes')
    ap.add_argument('--baseport', type=int, default=19000)
    ap.add_argument('--clients', type=int, default=200,
                    help='number of client nodes')
    ap.add_argument('--bin', type=str, required=True)
    ap.add_argument('--out', type=str, required=True)
    ap.add_argument('--connect', type=int, default=10)
    ap.add_argument('--ip', type=str, default=None)
    ap.add_argument('--ifname', type=str, default='lo')
    ap.add_argument('--netid', type=str, default=None)
    ap.add_argument('--loglevel', type=str, default='info')
    args = ap.parse_args()

    if args.valgrind:
        exe = 'valgrind {}'.format(args.bin)
    else:
        exe = '{} -v'.format(args.bin)
    basedir = os.path.abspath(args.dir)

    for nodeid in range(args.svc):
        config = CP()
        config['router'] = {
            'data-dir': '.',
            'net-threads': '1',
            'worker-threads': '4',
            'nickname': svcNodeName(nodeid),
            'min-connections': "{}".format(args.connect)
        }
        if args.netid:
            config['router']['netid'] = args.netid

        if args.ip:
            config['router']['public-ip'] = args.ip
            config['router']['public-port'] = str(args.baseport + nodeid)

        config['bind'] = {
            args.ifname: str(args.baseport + nodeid)
        }
        config["logging"] = {
            "level": args.loglevel
        }
        config['netdb'] = {
            'dir': 'netdb'
        }
        config['network'] = {
            'type' : 'null'
        }
        config['api'] = {
            'enabled': 'false'
        }
        config['lokid'] = {
            'enabled': 'false',
        }
        d = os.path.join(args.dir, svcNodeName(nodeid))
        if not os.path.exists(d):
            os.mkdir(d)
        fp = os.path.join(d, 'daemon.ini')
        with open(fp, 'w') as f:
            config.write(f)
            for n in [0]:
                if nodeid is not 0:
                    f.write("[bootstrap]\nadd-node={}\n".format(os.path.join(basedir,svcNodeName(n), 'self.signed')))
                else:
                    f.write("[bootstrap]\nseed-node=true\n")

    for nodeid in range(args.clients):
        config = CP()

        config['router'] = {
            'data-dir': '.',
            'net-threads': '1',
            'worker-threads': '2',
            'nickname': clientNodeName(nodeid)
        }
        if args.netid:
            config['router']['netid'] = args.netid

        config["logging"] = {
            "level": args.loglevel
        }

        config['netdb'] = {
            'dir': 'netdb'
        }
        config['api'] = {
            'enabled': 'false'
        }
        config['network'] = {
            'type' : 'null'
        }
        d = os.path.join(args.dir, clientNodeName(nodeid))
        if not os.path.exists(d):
            os.mkdir(d)
        fp = os.path.join(d, 'client.ini')
        with open(fp, 'w') as f:
            config.write(f)
            for n in [0]:
                otherID = (n + nodeid) % args.svc
                f.write("[bootstrap]\nadd-node={}\n".format(os.path.join(basedir,svcNodeName(otherID), 'self.signed')))

    with open(args.out, 'w') as f:
        basedir = os.path.join(args.dir, 'svc-node-%(process_num)03d')
        f.write('''[program:svc-node]
directory = {}
command = {} -r {}/daemon.ini
autorestart=true
redirect_stderr=true
#stdout_logfile=/dev/fd/1
stdout_logfile={}/svc-node-%(process_num)03d-log.txt
stdout_logfile_maxbytes=0
process_name = svc-node-%(process_num)03d
numprocs = {}
'''.format(basedir, exe, basedir, args.dir, args.svc))
        basedir = os.path.join(args.dir, 'client-node-%(process_num)03d')
        f.write('''[program:Client-node]
directory = {}
command = bash -c "sleep 5 && {} {}/client.ini"
autorestart=true
redirect_stderr=true
#stdout_logfile=/dev/fd/1
stdout_logfile={}/client-node-%(process_num)03d-log.txt
stdout_logfile_maxbytes=0
process_name = client-node-%(process_num)03d
numprocs = {}
'''.format(basedir, exe, basedir, args.dir, args.clients))
        f.write('[supervisord]\ndirectory=.\n')


if __name__ == '__main__':
    main()
