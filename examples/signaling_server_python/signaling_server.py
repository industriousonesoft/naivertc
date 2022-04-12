from http import client
import sys
import ssl
import json
import asyncio
import logging
import websockets

logger = logging.getLogger('websockets')
logger.setLevel(logging.INFO)
logger.addHandler(logging.StreamHandler(sys.stdout))

peers = {}

async def handle_websocket(websocket, path):
    peer_id = None
    try:
        while True:
                data = await websocket.recv()
                message = json.loads(data)
                type = message['type']
                if type == 'register':
                    print('Register: {}'.format(data))
                    peer_id = message['clientId']
                    if peer_id:
                        print('Peer {} registered.'.format(peer_id))
                        peers[peer_id] = websocket
                        is_initiator = False if len(peers) > 1 else True
                        res = {'type' : 'accept',
                               'isInitiator': is_initiator}
                        await websocket.send(json.dumps(res))
                    else:
                        res = {'type' : 'reject',
                               'reason': 'Faild to register without peer id.'}
                        await websocket.send(json.dumps(res))
                else:
                    for dest_peer_id in peers:
                        if peer_id == dest_peer_id:
                            continue
                        dest_websocket = peers[dest_peer_id]
                        if dest_websocket:
                            data = json.dumps(message)
                            print('Forward {} from {} to {}.'.format(type, peer_id, dest_peer_id))
                            await dest_websocket.send(data)
                        else:
                            print('Peer {} not found.'.format(dest_peer_id))
                    
    except Exception as e:
        print(e)
    finally:
        peers.clear()

if __name__ == '__main__':
    # Usage: ./signaling_server.py [[host:]port] [SSL certificate file]
    endpoint_or_port = sys.argv[1] if len(sys.argv) > 1 else '8000'
    ssl_cert = sys.argv[2] if len(sys.argv) > 2 else None

    endpoint = endpoint_or_port if ':' in endpoint_or_port else '127.0.0.1:' + endpoint_or_port

    if ssl_cert:
        ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ssl_ctx.load_cert_chain(ssl_cert)
    else:
        ssl_ctx = None

    print('Listening on {}'.format(endpoint))
    host, port = endpoint.rsplit(':', 1)
    start_server = websockets.serve(handle_websocket, host, int(port), ssl=ssl_ctx)
    asyncio.get_event_loop().run_until_complete(start_server)
    asyncio.get_event_loop().run_forever()