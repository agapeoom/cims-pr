
import asyncio
import aiohttp
import ssl
import json

async def test_csc():
    ssl_context = ssl.create_default_context()
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE

    async with aiohttp.ClientSession(connector=aiohttp.TCPConnector(ssl=ssl_context)) as session:
        base_url = "https://localhost:4420"
        
        print("--- Testing IdMS: Auth Req ---")
        async with session.get(f"{base_url}/idms/authreq", params={
            "client_id": "MCPTT_UE",
            "user_name": "tel:+1000",
            "user_password": "1234",
            "redirect_uri": "http://client/cb",
            "state": "mystate",
            "scope": "openid 3gpp:mcptt:ptt_server"
        }, allow_redirects=False) as resp:
            print(f"Status: {resp.status}")
            if resp.status == 302:
                location = resp.headers['Location']
                print(f"Location: {location}")
                # Extract code from location: http://client/cb?code=...&state=...
                import urllib.parse
                parsed = urllib.parse.urlparse(location)
                qs = urllib.parse.parse_qs(parsed.query)
                code = qs.get("code", [None])[0]
            else:
                data = await resp.json()
                code = data.get("code")
            
            assert code is not None
            
        print("\n--- Testing IdMS: Token Req ---")
        async with session.post(f"{base_url}/idms/tokenreq", data={
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": "http://client/cb",
            "client_id": "MCPTT_UE"
        }) as resp:
            print(f"Status: {resp.status}")
            token_data = await resp.json()
            print(f"Token Data keys: {token_data.keys()}")
            access_token = token_data.get("access_token")
            assert access_token is not None
            
        headers = {"Authorization": f"Bearer {access_token}"}
        
        print("\n--- Testing GMS: Group Retrieval ---")
        group_uri = "tel:+2000"
        async with session.get(f"{base_url}/org.openmobilealliance.groups/users/tel:+1000/{group_uri}", headers=headers) as resp:
             print(f"Status: {resp.status}")
             text = await resp.text()
             print(f"XML Preview: {text[:100]}...")
             assert resp.status == 200
             assert "group" in text

        print("\n--- Testing CMS: User Profile ---")
        async with session.get(f"{base_url}/org.3gpp.mcptt.user-profile/users/tel:+1000/user-profile", headers=headers) as resp:
             print(f"Status: {resp.status}")
             text = await resp.text()
             print(f"XML Preview: {text[:100]}...")
             assert resp.status == 200
             assert "MCPTTUserID" in text
             
        print("\n--- Testing KMS: Init ---")
        async with session.post(f"{base_url}/keymanagement/identity/v1/init", headers=headers) as resp:
             print(f"Status: {resp.status}")
             text = await resp.text()
             print(f"XML Preview: {text[:100]}...")
             assert resp.status == 200
             assert "KmsResponse" in text

if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.run_until_complete(test_csc())
