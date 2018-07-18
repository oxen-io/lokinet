#!/usr/bin/env python3
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
    ap.add_argument('--dir', type=str, default='testnet_tmp')
    ap.add_argument('--svc', type=int, default=20,
                    help='number of service nodes')
    ap.add_argument('--baseport', type=int, default=19000)
    ap.add_argument('--clients', type=int, default=200,
                    help='number of client nodes')
    ap.add_argument('--bin', type=str, required=True)
    ap.add_argument('--out', type=str, required=True)
    ap.add_argument('--connect', type=int, default=10)
    ap.add_argument('--ifname', type=str, default='lo')

    args = ap.parse_args()

    basedir = os.path.abspath(args.dir)

    for nodeid in range(args.svc):
        config = CP()
        config['router'] = {
            'net-threads': '1',
            'worker-threads': '4'
        }
        config['bind'] = {
            args.ifname: str(args.baseport + nodeid)
        }
        config['netdb'] = {
            'dir': 'netdb'
        }
        config['connect'] = {}
        for otherid in range(args.svc):
            if otherid != nodeid:
                name = svcNodeName(otherid)
                config['connect'][name] = os.path.join(
                    basedir, name, 'rc.signed')

        d = os.path.join(args.dir, svcNodeName(nodeid))
        if not os.path.exists(d):
            os.mkdir(d)
        fp = os.path.join(d, 'daemon.ini')
        with open(fp, 'w') as f:
            config.write(f)

    for nodeid in range(args.clients):
        config = CP()

        config['router'] = {
            'net-threads': '1',
            'worker-threads': '2'
        }
        config['netdb'] = {
            'dir': 'netdb'
        }
        config['connect'] = {}
        for otherid in range(args.connect):
            otherid = (nodeid + otherid) % args.svc
            name = svcNodeName(otherid)
            config['connect'][name] = os.path.join(
                basedir, name, 'rc.signed')

        d = os.path.join(args.dir, clientNodeName(nodeid))
        if not os.path.exists(d):
            os.mkdir(d)
        hiddenservice = os.path.join(d, 'service.ini')
        config['services'] = {
            'testnet': hiddenservice
        }
        fp = os.path.join(d, 'daemon.ini')
        with open(fp, 'w') as f:
            config.write(f)
        config = CP()
        config['test-service'] = {
            'tag': 'test',
            'prefetch-tag': "test"
        }
        with open(hiddenservice, 'w') as f:
            config.write(f)

    with open(args.out, 'w') as f:
        f.write('''[program:svc-node]
directory = {}
command = {}
redirect_stderr=true
stdout_logfile=/dev/fd/1
stdout_logfile_maxbytes=0
process_name = svc-node-%(process_num)03d
numprocs = {}
'''.format(os.path.join(args.dir, 'svc-node-%(process_num)03d'), args.bin, args.svc))
        f.write('''[program:client-node]
directory = {}
command = {}
redirect_stderr=true
stdout_logfile=/dev/fd/1
stdout_logfile_maxbytes=0
process_name = client-node-%(process_num)03d
numprocs = {}
'''.format(os.path.join(args.dir, 'client-node-%(process_num)03d'), args.bin, args.clients))
        f.write('[supervisord]\ndirectory=.\n')


if __name__ == '__main__':
    main()
