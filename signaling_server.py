import sys
import ssl
import json
import websockets
import argparse
import asyncio
import signal
from websockets.asyncio.server import serve


clients = {}

async def handler(websocket):
    client_id = None
    try:
        client_id = websocket.request.path.split('/')[1]
        print('Creatting connection with clinent: ', client_id)

        while True:
            data = await websocket.recv()
            message = json.loads(data)
            to_id = message.get('id')

            if to_id == client_id:
                continue

            to_websocket = clients.get(to_id)

            if to_websocket is None:
                print('Connection with client ', to_id, ' not found.')
                continue
            
            message['id'] = client_id
            data = json.dumps(message)
            to_websocket.send(data)
            print('Send message to client:\nfrom {} to {}\n'.format(client_id, to_id))
    except Exception as e:
        print(e)
    finally:
        if client_id:
            del clients[client_id]
            print('Client {} disconnected'.format(client_id))
            



# signaling_server.py [[host:]port] [ssl]
async def main():
    # parsing command line args
    parser = argparse.ArgumentParser()
    parser.add_argument('-s', '--host', default='127.0.0.1', type=str)
    parser.add_argument('-p', '--port', default=8000, type=int)
    args = parser.parse_args()

    # creating websocket
    server = await websockets.serve(handler, args.host, args.port)
    print('Server listening on {}:{}'.format(args.host, args.port))

    # preparing for graceful shutdown
    stop = asyncio.Future()
    def signal_handler():
        if not stop.done():
            stop.set_result(True)
        return

    loop = asyncio.get_running_loop()
    loop.add_signal_handler(signal.SIGINT, signal_handler)
    loop.add_signal_handler(signal.SIGTERM, signal_handler)    

    await stop

    print('Graceful shutdown...')

    server.close()
    await server.wait_closed()

    print('Server closed')



if __name__ == '__main__':
    asyncio.run(main())