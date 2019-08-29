#!/usr/bin/env python3

import configparser
import sys
import os

from xml.etree import ElementTree as etree


def getSetting(s, name, fallback): return name in s and s[name] or fallback


shadowRoot = getSetting(os.environ, "SHADOW_ROOT",
                        os.path.join(os.environ['HOME'], '.shadow'))

libpath = 'libshadow-plugin-lokinet.so'


def nodeconf(conf, baseDir, name, ifname=None, port=None):
    conf['netdb'] = {'dir': 'tmp-nodes'}
    conf['router'] = {}
    conf['router']['contact-file'] = os.path.join(
        baseDir, '{}.signed'.format(name))
    conf['router']['ident-privkey'] = os.path.join(
        baseDir, '{}-ident.key'.format(name))
    conf['router']['transport-privkey'] = os.path.join(
        baseDir, '{}-transport.key'.format(name))
    if ifname and port:
        conf['bind'] = {ifname: port}
    conf['connect'] = {}


def addPeer(conf, baseDir, peer):
    conf['connect'][peer] = os.path.join(baseDir, '{}.signed'.format(peer))


def createNode(pluginName, root, peer, life=600):
    node = etree.SubElement(root, 'node')
    node.attrib['id'] = peer['name']
    node.attrib['interfacebuffer'] = '{}'.format(1024 * 1024 * 100)
    app = etree.SubElement(node, 'process')
    app.attrib['plugin'] = pluginName
    app.attrib['time'] = '{}'.format(life)
    app.attrib['arguments'] = peer['configfile']


def makeBase(settings, name, id):
    return {
        'id': id,
        'name': name,
        'contact-file': os.path.join(getSetting(settings, 'baseDir', 'tmp'), '{}.signed'.format(name)),
        'configfile': os.path.join(getSetting(settings, 'baseDir', 'tmp'), '{}.ini'.format(name)),
        'config': configparser.ConfigParser()
    }


def makeClient(settings, name, id):
    peer = makeBase(settings, name, id)
    basedir = getSetting(settings, 'baseDir', 'tmp')
    nodeconf(peer['config'], basedir, name)
    peer['config']['network'] = {
        'type': 'null',
        'tag': 'test',
        'prefetch-tag': 'test'
    }
    return peer


def makeSVCNode(settings, name, id, port):
    peer = makeBase(settings, name, id)
    nodeconf(peer['config'], getSetting(
        settings, 'baseDir', 'tmp'), name, 'eth0', port)
    peer['config']['network'] = {
        'type': 'null'
    }
    return peer


def genconf(settings, outf):
    root = etree.Element('shadow')
    root.attrib["environment"] = 'LLARP_SHADOW=1'
    topology = etree.SubElement(root, 'topology')
    topology.attrib['path'] = getSetting(settings, 'topology', os.path.join(
        shadowRoot, 'share', 'topology.graphml.xml'))

    pluginName = getSetting(settings, 'name', 'lokinet-shared')

    kill = etree.SubElement(root, 'kill')
    kill.attrib['time'] = getSetting(settings, 'runFor', '600')

    baseDir = getSetting(settings, 'baseDir',
                         os.path.join('/tmp', 'lokinet-shadow'))

    if not os.path.exists(baseDir):
        os.mkdir(baseDir)

    plugin = etree.SubElement(root, "plugin")
    plugin.attrib['id'] = pluginName
    plugin.attrib['path'] = libpath
    basePort = getSetting(settings, 'svc-base-port', 19000)
    svcNodeCount = getSetting(settings, 'service-nodes', 80)
    peers = list()
    for nodeid in range(svcNodeCount):
        peers.append(makeSVCNode(
            settings, 'svc-node-{}'.format(nodeid), str(nodeid), basePort + 1))
        basePort += 1

    # make all service nodes know each other
    for peer in peers:
        for nodeid in range(svcNodeCount):
            if str(nodeid) != peer['id']:
                addPeer(peer['config'], baseDir, 'svc-node-{}'.format(nodeid))

    # add client nodes
    for nodeid in range(getSetting(settings, 'client-nodes', 200)):
        peer = makeClient(
            settings, 'client-node-{}'.format(nodeid), str(nodeid))
        peers.append(peer)
        for p in range(getSetting(settings, 'client-connect-to', 10)):
            addPeer(peer['config'], baseDir,
                    'svc-node-{}'.format((p + nodeid) % svcNodeCount))

    # generate xml and settings files
    for peer in peers:
        createNode(pluginName, root, peer)

        with open(peer['configfile'], 'w') as f:
            peer['config'].write(f)

    # render
    outf.write(etree.tostring(root).decode('utf-8'))


if __name__ == '__main__':
    settings = {
        'baseDir': os.path.join("/tmp", "lokinet-shadow"),
        'topology': os.path.join(shadowRoot, 'share', 'topology.graphml.xml'),
        'runFor': '{}'.format(60 * 10 * 10)
    }
    with open(sys.argv[1], 'w') as f:
        genconf(settings, f)
