
from aiohttp import web
import urllib.parse
from csc_data import USERS, GROUPS, TOKENS, create_tokens, validate_access_token, get_group_xml, get_user_profile_xml, get_service_config_xml, get_kms_init_xml, get_kms_keyprov_xml

auth_codes = {}

# Middleware for Access Token Validation
async def auth_middleware(app, handler):
    async def middleware_handler(request):
        # Skip Auth for IdMS endpoints
        if request.path.startswith('/idms/'):
            return await handler(request)
            
        auth_header = request.headers.get('Authorization') or request.headers.get('access_token')
        
        if not auth_header:
             return web.Response(status=401, text='Missing Authorization Header')

        token = auth_header.replace('Bearer ', '')
        payload = validate_access_token(token)
        
        if not payload:
            return web.Response(status=401, text='Invalid Token', headers={"WWW-Authenticate": 'Bearer realm="", error="invalid_token", error_description="access_token_expired"'})
            
        request['user'] = payload
        return await handler(request)
        
    return middleware_handler

# IdMS Handlers
async def handle_auth_req(request):
    params = request.query
    client_id = params.get('client_id')
    user_name = params.get('user_name')
    user_password = params.get('user_password')
    redirect_uri = params.get('redirect_uri')
    state = params.get('state')
    
    print(f"[IdMS] Auth Req: user={user_name}")

    if user_name not in USERS or USERS[user_name]['password'] != user_password:
        return web.HTTPFound(location=f"{redirect_uri}?error=auth_fail&state={state}")

    # Generate Auth Code
    code = f"auth_code_{int(time.time())}"
    auth_codes[code] = {"user_id": user_name, "scope": params.get('scope')}
    
    response_data = {
        "Location": redirect_uri,
        "code": code,
        "state": state
    }
    return web.json_response(response_data)

import time

async def handle_token_req(request):
    data = await request.post()
    if not data:
        try:
            data = await request.json()
        except:
            pass
        
    grant_type = data.get('grant_type')
    code = data.get('code')
    print(f"[IdMS] Token Req: grant_type={grant_type}, code={code}")
    
    if grant_type == 'authorization_code':
        if code in auth_codes:
            user_data = auth_codes.pop(code)
            id_token, access_token, refresh_token = create_tokens(user_data['user_id'], user_data['scope'])
            
            return web.json_response({
                "access_token": access_token,
                "refresh_token": refresh_token,
                "id_token": id_token,
                "token_type": "Bearer",
                "expires_in": 3600
            })
        else:
             return web.json_response({"error": "invalid_grant"}, status=400)
             
    elif grant_type == 'refresh_token':
         refresh_token = data.get('refresh_token')
         if refresh_token in TOKENS:
             token_data = TOKENS[refresh_token]
             id_token, access_token, new_refresh_token = create_tokens(token_data['user_id'], token_data['scope'])
             # Invalidate old refresh token (optional, simple rotation)
             del TOKENS[refresh_token]
             
             return web.json_response({
                "access_token": access_token,
                "refresh_token": new_refresh_token,
                "token_type": "Bearer",
                "expires_in": 3600
            })
         else:
             return web.json_response({"error": "invalid_grant"}, status=400)
             
    return web.json_response({"error": "unsupported_grant_type"}, status=400)

# GMS Handlers
async def handle_group_retrieval(request):
    # URI format: /org.openmobilealliance.groups/users/{user_id}/{group_id}
    # Example: /org.openmobilealliance.groups/users/tel:+82510200011/tel:+8293315310133
    
    path = request.path
    parts = path.split('/')
    group_uri = parts[-1]
    
    print(f"[GMS] Group Retrieval: {group_uri}")
    
    xml, etag = get_group_xml(group_uri)
    
    if xml:
        return web.Response(text=xml, content_type='application/vnd.oma.poc.groups+xml', headers={'Etag': etag})
    else:
        return web.Response(status=404)

# CMS Handlers
async def handle_user_profile(request):
    # URI: /org.3gpp.mcptt.user-profile/users/{user_id}/user-profile
    path = request.path
    # Extract user_id. Simple hack: assume it is between 'users/' and '/user-profile'
    try:
        start = path.find('/users/') + 7
        end = path.find('/user-profile', start)
        user_uri = path[start:end]
    except:
        return web.Response(status=400)

    print(f"[CMS] User Profile: {user_uri}")
    xml, etag = get_user_profile_xml(user_uri)
    
    if xml:
        return web.Response(text=xml, content_type='application/vnd.3gpp.mcptt-user-profile+xml', headers={'Etag': etag})
    else:
        return web.Response(status=404)

async def handle_service_config(request):
    # URI: /org.3gpp.mcptt.service-config/users/{user_id}/service-config
    path = request.path
    try:
        start = path.find('/users/') + 7
        end = path.find('/service-config', start)
        user_uri = path[start:end]
    except:
        return web.Response(status=400)
        
    print(f"[CMS] Service Config: {user_uri}")
    xml, etag = get_service_config_xml(user_uri)
    
    if xml:
         return web.Response(text=xml, content_type='application/vnd.3gpp.mcptt-service-config+xml', headers={'Etag': etag})
    else:
        return web.Response(status=404)

# KMS Handlers
async def handle_kms_init(request):
    token = request['user']
    user_uri = token.get('mcptt_id')
    print(f"[KMS] Init: {user_uri}")
    
    xml = get_kms_init_xml(user_uri)
    print("KMS Init Request : ", request)
    print("KMS Init XML: ", xml)
    return web.Response(text=xml, content_type='application/xml')

async def handle_kms_keyprov(request):
    token = request['user']
    user_uri = token.get('mcptt_id')
    print(f"[KMS] KeyProv: {user_uri}")
    
    xml = get_kms_keyprov_xml(user_uri)
    return web.Response(text=xml, content_type='application/xml')

def setup_routes(app):
    app.router.add_get('/idms/authreq', handle_auth_req)
    app.router.add_post('/idms/tokenreq', handle_token_req)
    app.router.add_post('/idms/tokenrefresh', handle_token_req)
    
    # Wildcard for XCAP as they have dynamic paths
    app.router.add_get('/org.openmobilealliance.groups/users/{user_id}/{group_id}', handle_group_retrieval)
    
    # CMS
    app.router.add_get('/org.3gpp.mcptt.user-profile/users/{user_id}/user-profile', handle_user_profile)
    app.router.add_get('/org.3gpp.mcptt.user-profile/users/{user_id}/user-profile/{element}', handle_user_profile) # Simple support for sub-elements
    
    app.router.add_get('/org.3gpp.mcptt.service-config/users/{user_id}/service-config', handle_service_config)

    # KMS
    app.router.add_post('/keymanagement/identity/v1/init', handle_kms_init)
    app.router.add_post('/keymanagement/identity/v1/keyprov', handle_kms_keyprov)
