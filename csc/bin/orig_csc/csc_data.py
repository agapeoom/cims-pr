
import jwt
import time
import datetime
import uuid

# Configuration
SECRET_KEY = "my_secret_key"
IDMS_ISSUER = "idms.mcptt.com"
KMS_URI = "kms.mcptt.com"

import os
import json
import glob

# Mock Data (Will be overwritten by load_shared_data)
USERS = {}
GROUPS = {}

def load_shared_data(config):
    # Do not reassign global variables, modify them in place
    USERS.clear()
    GROUPS.clear()
    
    user_path = config['Data']['User']
    group_path = config['Data']['Group']
    
    # Load Users: ../user/XX/XX/XX/XX/XXXXXXXXXX.json
    # Pattern: user_path/**/*.json
    print(f"Loading users from {user_path}...")
    user_files = glob.glob(os.path.join(user_path, '**', '*.json'), recursive=True)
    for fpath in user_files:
        try:
            with open(fpath, 'r') as f:
                data = json.load(f)
                # CSP User JSON: {"passwd": "...", "org_id": "...", ...} (ID implied from filename)
                filename = os.path.basename(fpath).split('.')[0] # e.g., 0000001000
                user_id = str(int(filename)) # Remove leading zeros if treated as int, or keep string? CSP IDs usually strings.
                uri = f"tel:+{user_id}"
                
                USERS[uri] = {
                    "password": data.get('passwd', 'password123'),
                    "name": data.get('name', 'Unknown User'),
                    "profile_etag": "etag_" + uri
                }
                print(f"Loaded User: {uri}")
                
        except Exception as e:
            print(f"Error loading user {fpath}: {e}")

    # Load Groups: ../group/*.json
    print(f"Loading groups from {group_path}...")
    group_files = glob.glob(os.path.join(group_path, '*.json'))
    for fpath in group_files:
        try:
             with open(fpath, 'r') as f:
                data = json.load(f)
                # CSP Group JSON: {"name": "...", "users": [{"id": ...}]}
                # ID from filename e.g. 2000.json -> 2000
                group_id = os.path.basename(fpath).split('.')[0]
                uri = f"tel:+{group_id}"
                print(f"Loaded Group: {uri}")
                
                members = []
                for m in data.get('users', []):
                     # CSP member object: {"id": "2001", "priority": 0}
                     m_id = m.get('id')
                     if m_id:
                         m_uri = f"tel:+{m_id}"
                         members.append({"uri": m_uri, "name": m_uri, "role": "participant"})

                GROUPS[uri] = {
                    "display_name": data.get('name', 'Group'),
                    "etag": "etag_" + uri,
                    "members": members
                }
        except Exception as e:
             print(f"Error loading group {fpath}: {e}")

TOKENS = {}

def create_tokens(user_id, scope):
    now = int(time.time())
    
    # ID Token
    id_token_payload = {
        "mcptt_id": user_id,
        "iss": IDMS_ISSUER,
        "sub": str(uuid.uuid4()),
        "aud": "mcptt_client",
        "exp": now + 3600,
        "iat": now
    }
    id_token = jwt.encode(id_token_payload, SECRET_KEY, algorithm="HS256")
    
    # Access Token
    access_token_payload = {
        "mcptt_id": user_id,
        "aud": "mcptt_client",
        "exp": now + 3600,
        "scope": scope.split()
    }
    access_token = jwt.encode(access_token_payload, SECRET_KEY, algorithm="HS256")
    
    # Refresh Token
    refresh_token = str(uuid.uuid4())
    TOKENS[refresh_token] = {
        "user_id": user_id,
        "scope": scope,
        "exp": now + 7200
    }
    
    return id_token, access_token, refresh_token

def validate_access_token(token):
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=["HS256"], options={"verify_signature": True}, audience="mcptt_client")
        return payload
    except Exception as e:
        print(f"Token validation error: {e}")
        return None

def get_group_xml(group_uri):
    group = GROUPS.get(group_uri)
    if not group:
        return None, None

    xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<group xmlns="urn:oma:xml:poc:list-service"
  xmlns:rl="urn:ietf:params:xml:ns:resource-lists"
  xmlns:cp="urn:ietf:params:xml:ns:common-policy"
  xmlns:ocp="urn:oma:xml:xdm:common-policy"
  xmlns:oxe="urn:oma:xml:xdm:extensions"
  xmlns:mcpttgi="urn:3gpp:ns:mcpttGroupInfo:1.0">
  <list-service uri="{group_uri}">
    <display-name xml:lang="en-us">{group['display_name']}</display-name>
    <list>"""
    
    for member in group['members']:
        xml += f"""
      <entry uri="{member['uri']}">
        <rl:display-name>{member['name']}</rl:display-name>
        <mcpttgi:on-network-required/>
        <mcpttgi:user-priority>1</mcpttgi:user-priority>
      </entry>"""
      
    xml += """
    </list>
    <mcpttgi:on-network-invite-members>true</mcpttgi:on-network-invite-members>
    <mcpttgi:on-network-max-participant-count>10</mcpttgi:on-network-max-participant-count>
    <cp:ruleset>
      <cp:rule id="a7c">
         <cp:actions>
          <mcpttgi:allow-MCPTT-emergency-call>true</mcpttgi:allow-MCPTT-emergency-call>
          <mcpttgi:allow-imminent-peril-call>true</mcpttgi:allow-imminent-peril-call>
          <mcpttgi:allow-MCPTT-emergency-alert>true</mcpttgi:allow-MCPTT-emergency-alert>
        </cp:actions>
      </cp:rule>
    </cp:ruleset>
    <oxe:supported-services>
     <oxe:service enabler="example.mcptt">
      <oxe:group-media>
       <mcpttgi:mcptt-speech/>
      </oxe:group-media>
     </oxe:service>
    </oxe:supported-services>
    <mcpttgi:on-network-group-priority>5</mcpttgi:on-network-group-priority>
  </list-service>
</group>"""
    return xml, group['etag']

def get_user_profile_xml(user_uri):
    user = USERS.get(user_uri)
    if not user:
        return None, None
        
    xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<MCPTTUserID index="{user_uri}">
    <uri-entry>http://csc.mcptt.com/org.3gpp.mcptt.user-profile/users/{user_uri}</uri-entry>
    <display-name xml:lang="en-us">{user_uri}</display-name>
</MCPTTUserID>"""
    return xml, user['profile_etag']

def get_service_config_xml(user_uri):
    # Mock service config
    xml = """<?xml version="1.0" encoding="UTF-8"?>
<num-levels-group-hierarchy>3</num-levels-group-hierarchy>
<num-levels-user-hierarchy>3</num-levels-user-hierarchy>"""
    return xml, "VTGvRnMgDsXzkhmnQ8HIETX9ZsidQRLv"

def get_kms_init_xml(user_uri):
    # Mock KMS Init response
    now = datetime.datetime.now(datetime.timezone.utc)
    valid_from = now.isoformat()
    valid_to = (now + datetime.timedelta(days=365*20)).isoformat()
    
    xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<KmsResponse Version="1.1.0" xmlns="http://org.csc.kms" xmlns:ds="http://www.w3.org/2000/09/xmldsig#">
<KmsUri>{KMS_URI}</KmsUri>
<UserUri>{user_uri}@ps-lte.com</UserUri>
<Time>{now.isoformat()}</Time>
<KmsId>kmsprovider12345</KmsId>
<ClientReqUrl>http://10.121.2.90/keymanagement/identity/v1/init</ClientReqUrl>
<KmsMessage>
<KmsInit Version="1.0.0">
<KmsCertificate Version="1.0.0" Role="Root">
<KmsUri>{KMS_URI}</KmsUri>
<Issuer>www.mcptt.com</Issuer>
<ValidFrom>{valid_from}</ValidFrom>
<ValidTo>{valid_to}</ValidTo>
<UserIdFormat>2</UserIdFormat>
<UserKeyPeriod>2419200</UserKeyPeriod>
<UserKeyOffset>0</UserKeyOffset>
<PubEncKey>041C7B84B4FD620D49F3DC2366A7F62F48221D7B32D61D2A16685A015FDACF03CDDBAA66B78C597410C290EE3E8D7FE950193B87DABD3A33180DCEEF66893B24504EA22C9C7FD46BDCD385AF14EC71A57F94363692FA7FE0CE931BCF7A4F95A32723A459AC0ED72ECF17A8E9E2EBF94976E493134F5D11EE3D42165B5EF6E22FDD5269CBD01D339A5768521E36E1A1BEF2EC0D4B2606943DFAFB010A806F553E81350039EABD25FBF0758F25FC38E730553C19675B796DFE005C16696B3879388547282B3A3F56ADA1EA3C01AF77DE412EA62D4676D2386F745304B8B3AD63BB8E4E01C3C342B984B57512EA58A5049CE04BA2D00A36A3C78F46A364A670DE9F64</PubEncKey>
<PubAuthKey>0467EF33902289EA2F42A82912CFD12B517A321EED22D56EB9B5AA60A3A38F97B77A29B3875339F141E454E3A9CF53A3C0353B1A88868A39A15D74A7B235E09EB8</PubAuthKey>
<ParameterSet>1</ParameterSet>
</KmsCertificate>
</KmsInit>
</KmsMessage>
</KmsResponse>"""
    return xml

def get_kms_keyprov_xml(user_uri):
     # Mock KMS KeyProv response
    now = datetime.datetime.now(datetime.timezone.utc)
    
    xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<KmsResponse Version="1.1.0" xmlns="http://org.csc.kms" xmlns:ds="http://www.w3.org/2000/09/xmldsig#">
<KmsUri>{KMS_URI}</KmsUri>
<UserUri>{user_uri}@ptt.mnc031.mcc450.3gppnetwork.org</UserUri>
<Time>{now.isoformat()}</Time>
<KmsId>kmsprovider12345</KmsId>
<ClientReqUrl>http://10.121.2.90/keymanagement/identity/v1/init</ClientReqUrl>
<KmsMessage>
<KmsInit Version="1.0.0">
<KmsCertificate Version="1.0.0" Role="Root">
<KmsUri>{KMS_URI}</KmsUri>
<Issuer>www.mcptt.com</Issuer>
<ValidFrom>2016-11-01T00:00:00+09:00</ValidFrom>
<ValidTo>2036-10-31T23:59:59+09:00</ValidTo>
<UserIdFormat>2</UserIdFormat>
<UserKeyPeriod>2419200</UserKeyPeriod>
<UserKeyOffset>0</UserKeyOffset>
<PubEncKey>041C7B84B4FD620D49F3DC2366A7F62F48221D7B32D61D2A16685A015FDACF03CDDBAA66B78C597410C290EE3E8D7FE950193B87DABD3A33180DCEEF66893B24504EA22C9C7FD46BDCD385AF14EC71A57F94363692FA7FE0CE931BCF7A4F95A32723A459AC0ED72ECF17A8E9E2EBF94976E493134F5D11EE3D42165B5EF6E22FDD5269CBD01D339A5768521E36E1A1BEF2EC0D4B2606943DFAFB010A806F553E81350039EABD25FBF0758F25FC38E730553C19675B796DFE005C16696B3879388547282B3A3F56ADA1EA3C01AF77DE412EA62D4676D2386F745304B8B3AD63BB8E4E01C3C342B984B57512EA58A5049CE04BA2D00A36A3C78F46A364A670DE9F64</PubEncKey>
<PubAuthKey>0467EF33902289EA2F42A82912CFD12B517A321EED22D56EB9B5AA60A3A38F97B77A29B3875339F141E454E3A9CF53A3C0353B1A88868A39A15D74A7B235E09EB8</PubAuthKey>
<ParameterSet>1</ParameterSet>
</KmsCertificate>
</KmsInit>
</KmsMessage>
</KmsResponse>"""
    return xml
