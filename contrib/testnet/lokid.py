#!/usr/bin/env python3


import os
from base64 import b64decode

from flask import Flask, jsonify, request, abort, Response, make_response
from nacl import signing
from nacl import encoding

class SVCNode:
    """
    info on a service node
    """
    def __init__(self):
        self._seed = os.urandom(32)
        self._ed25519_secret = signing.SigningKey(self._seed)

    def seed(self):
        """
        return hex seed
        """
        return self._ed25519_secret.encode(encoding.HexEncoder).decode('ascii') + self.pubkey()

    def pubkey(self):
        """
        make hex public key
        """
        return self._ed25519_secret.verify_key.encode(encoding.HexEncoder).decode('ascii')

    def toJson(self):
        """
        make the snode a json object for jsonrpc
        """
        return {'pubkey_ed22519': self.pubkey(), 'active': True, 'funded': True}

class MockServer:

    def __init__(self, numServiceNodes):
        self.app = Flask('lokid-rpc-mock')
        #self.app.config['SECRET_KEY'] = os.urandom(16)
        # populate service nodes
        self._serviceNodes = dict()
        for n in range(numServiceNodes):
            self.makeSNode("svc-%03d" % n)

        self._handlers = {
            'lokinet_ping': self._lokinet_ping,
            'get_n_service_nodes' : self._get_n_service_nodes,
            'get_service_node_privkey' : self._get_service_node_privkey
        }
        #digest = HTTPDigestAuth(realm='lokid')
    
        @self.app.route('/json_rpc', methods=["POST"])
        def _jsonRPC():
            j = request.get_json()
            method = j['method']
            snode = None
            if 'authorization' in request.headers:
                user = b64decode(request.headers['authorization'][6:].encode('ascii')).decode('ascii').split(':')[0]
                self.app.logger.error(user)
                if len(user) > 0:
                    snode = self._serviceNodes[user]
            result = self._handlers[method](snode)
            if result:
                resp = {'jsonrpc': '2.0', 'id': j['id'], 'result': result}
                return jsonify(resp)
            else:
                r = make_response('nope', 401)
                r.headers['www-authenticate'] = 'basic'
                return r
        def after(req):
            req.content_type = "application/json"
            return req
        self.app.after_request(after)

    def _get_n_service_nodes(self, our_snode):
        return {
            'block_hash' : 'mock',
            'service_node_states' : self.getSNodeList()
        }
    
    def _get_service_node_privkey(self, our_snode):
        if our_snode is None:
            return None
        return {
            'service_node_ed25519_privkey': our_snode.seed()
        }

    def _lokinet_ping(self, snode):
        return {
            'status' : "OK"
        }
    
    def run(self):
        """
        run mainloop and serve jsonrpc server
        """
        self.app.run()
    
    def makeSNode(self, name):
        """
        make service node entry
        """
        self._serviceNodes[name] = SVCNode()


    def getSNodeList(self):
        l = list()
        for name in self._serviceNodes:
            l.append(self._serviceNodes[name].toJson())
        return l
    

def main():
    import sys
    serv = MockServer(int(sys.argv[1]))
    serv.run()
    
if __name__ == '__main__':
    main()
