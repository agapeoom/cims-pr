
import ssl
from aiohttp import web
from csc_routes import setup_routes, auth_middleware

from csc_data import load_shared_data

def init_app():
    # Helper to load data from config available in app context later, 
    # but simplest is to just load it globally for this refactor
    # We will call load_shared_data from main before creating app
    app = web.Application(middlewares=[auth_middleware])
    setup_routes(app)
    return app

import json
import logging

def load_config():
    with open('../config/csc.json', 'r') as f:
        return json.load(f)

def setup_logging(config):
    log_file = config['Log']['File']
    level_str = config['Log']['Level']
    level = getattr(logging, level_str.upper(), logging.INFO)
    logging.basicConfig(filename=log_file, level=level, 
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')

if __name__ == '__main__':
    config = load_config()
    setup_logging(config)
    
    if 'Data' in config:
        load_shared_data(config)
        
    server_conf = config['Server']
    
    # SSL Context
    ssl_context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
    ssl_context.load_cert_chain(certfile=server_conf['CertFile'], keyfile=server_conf['KeyFile'])
    
    app = init_app()
    app['config'] = config # Store config in app
    
    port = server_conf['Port']
    print(f"Starting CSC Server on https://{server_conf['Ip']}:{port}")
    web.run_app(app, host=server_conf['Ip'], port=port, ssl_context=ssl_context)
