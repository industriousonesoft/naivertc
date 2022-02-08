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

clients = {}

async def handle_websocket(websocket, path):
    client_id = None
    try:
        splitted = path.split('/')
        splitted.pop(0)
        client_id = splitted.pop(0)
        print('Client {} connected.'.format(client_id))

        clients[client_id] = websocket
        while True:
            data = await websocket.recv()
            print('Client {} << {}'.format(client_id, data))
            message = json.loads(data)
            # TODO: Using roomId instead.
            dest_client_id = message['id']
            dest_websocket = clients[dest_client_id]
            if dest_websocket:
                message['id'] = client_id
                data = json.dumps(message)
                print('Client {} >> {}'.format(dest_client_id, data))
                await dest_websocket.send(data)
            else:
                print('Client {} not found'.format(dest_client_id))

    except Exception as e:
        print(e)
    finally:
        if client_id:
            del clients[client_id]
            print('Client {} disconnected'.format(client_id))


if __name__ == '__main__':
    # Usage: ./signaling_server.py [[host:]port] [SSL certificate file]
    endpoint_or_port = sys.argv[1] if len(sys.argv) > 1 else '8000'
    ssl_cert = sys.argv[2] if len(sys.argv) > 2 else None

    endpoint = endpoint_or_port if ':' in endpoint_or_port else '127.0.0.1' + endpoint_or_port

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