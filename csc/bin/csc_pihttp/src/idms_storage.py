"""
IDMS Storage Module
OAuth 2.0 / OIDC 표준을 위한 영속성 저장 모듈

기능:
- auth-code 저장/조회/삭제
- refresh-token 저장/조회/회수
- 원자적 파일 저장 (atomic write)
- 파일 락 (file locking)
- 만료된 항목 자동 정리
"""

import os
import json
import time
import fcntl
import tempfile
from typing import Dict, Optional, List
from util.log_util import Logger

logger = Logger()

class IdmsStorage:
    def __init__(self, data_dir: str = "data"):
        """
        초기화
        
        Args:
            data_dir: 데이터 파일이 저장될 디렉토리
        """
        self.data_dir = data_dir
        self.auth_codes_file = os.path.join(data_dir, "idms_auth_codes.json")
        self.refresh_tokens_file = os.path.join(data_dir, "idms_refresh_tokens.json")
        self.lock_file = os.path.join(data_dir, ".idms.lock")
        
        # 디렉토리 생성
        os.makedirs(data_dir, exist_ok=True)
        
        # 파일 초기화
        self._init_file(self.auth_codes_file, {"version": 1, "codes": {}})
        self._init_file(self.refresh_tokens_file, {"version": 1, "tokens": {}})
    
    def _init_file(self, filepath: str, default_data: dict):
        """파일이 없으면 기본 데이터로 생성"""
        if not os.path.exists(filepath):
            self._atomic_write(filepath, default_data)
            logger.log_info(f"Initialized {filepath}")
    
    def _atomic_write(self, filepath: str, data: dict):
        """
        원자적 파일 저장
        
        1. tmp 파일에 write
        2. fsync
        3. rename (원자적)
        """
        dir_path = os.path.dirname(filepath)
        
        # 임시 파일 생성
        fd, tmp_path = tempfile.mkstemp(dir=dir_path, prefix=".tmp_", suffix=".json")
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
                f.flush()
                os.fsync(f.fileno())  # 디스크에 강제 쓰기
            
            # 원자적 교체
            os.replace(tmp_path, filepath)
        except Exception as e:
            # 실패 시 임시 파일 삭제
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)
            raise e
    
    def _read_with_lock(self, filepath: str) -> dict:
        """파일 락을 사용하여 안전하게 읽기"""
        with open(self.lock_file, 'w') as lock_f:
            fcntl.flock(lock_f.fileno(), fcntl.LOCK_EX)
            try:
                if not os.path.exists(filepath):
                    return {"version": 1, "codes": {}} if "auth_codes" in filepath else {"version": 1, "tokens": {}}
                
                with open(filepath, 'r', encoding='utf-8') as f:
                    return json.load(f)
            finally:
                fcntl.flock(lock_f.fileno(), fcntl.LOCK_UN)
    
    def _write_with_lock(self, filepath: str, data: dict):
        """파일 락을 사용하여 안전하게 쓰기"""
        with open(self.lock_file, 'w') as lock_f:
            fcntl.flock(lock_f.fileno(), fcntl.LOCK_EX)
            try:
                self._atomic_write(filepath, data)
            finally:
                fcntl.flock(lock_f.fileno(), fcntl.LOCK_UN)
    
    # ==================== Auth Code 관리 ====================
    
    def save_auth_code(self, code: str, data: dict) -> bool:
        """
        auth-code 저장
        
        Args:
            code: authorization code
            data: {
                "user_id": str,
                "client_id": str,
                "redirect_uri": str,
                "scope": str,
                "state": str,
                "code_challenge": str (optional),
                "code_challenge_method": str (optional),
                "issued_at": int,
                "expires_at": int,
                "used": bool
            }
        """
        try:
            file_data = self._read_with_lock(self.auth_codes_file)
            file_data["codes"][code] = data
            self._write_with_lock(self.auth_codes_file, file_data)
            logger.log_info(f"Saved auth-code: {code}")
            return True
        except Exception as e:
            logger.log_error(f"Failed to save auth-code: {e}")
            return False
    
    def get_auth_code(self, code: str) -> Optional[dict]:
        """auth-code 조회"""
        try:
            file_data = self._read_with_lock(self.auth_codes_file)
            return file_data["codes"].get(code)
        except Exception as e:
            logger.log_error(f"Failed to get auth-code: {e}")
            return None
    
    def delete_auth_code(self, code: str) -> bool:
        """auth-code 삭제"""
        try:
            file_data = self._read_with_lock(self.auth_codes_file)
            if code in file_data["codes"]:
                del file_data["codes"][code]
                self._write_with_lock(self.auth_codes_file, file_data)
                logger.log_info(f"Deleted auth-code: {code}")
                return True
            return False
        except Exception as e:
            logger.log_error(f"Failed to delete auth-code: {e}")
            return False
    
    def mark_auth_code_used(self, code: str) -> bool:
        """auth-code를 사용됨으로 표시"""
        try:
            file_data = self._read_with_lock(self.auth_codes_file)
            if code in file_data["codes"]:
                file_data["codes"][code]["used"] = True
                self._write_with_lock(self.auth_codes_file, file_data)
                logger.log_info(f"Marked auth-code as used: {code}")
                return True
            return False
        except Exception as e:
            logger.log_error(f"Failed to mark auth-code: {e}")
            return False
    
    def cleanup_expired_codes(self) -> int:
        """만료된 auth-code 정리"""
        try:
            now = int(time.time())
            file_data = self._read_with_lock(self.auth_codes_file)
            
            expired_codes = [
                code for code, data in file_data["codes"].items()
                if data.get("expires_at", 0) < now
            ]
            
            for code in expired_codes:
                del file_data["codes"][code]
            
            if expired_codes:
                self._write_with_lock(self.auth_codes_file, file_data)
                logger.log_info(f"Cleaned up {len(expired_codes)} expired auth-codes")
            
            return len(expired_codes)
        except Exception as e:
            logger.log_error(f"Failed to cleanup auth-codes: {e}")
            return 0
    
    # ==================== Refresh Token 관리 ====================
    
    def save_refresh_token(self, token: str, data: dict) -> bool:
        """
        refresh-token 저장
        
        Args:
            token: refresh token (UUID)
            data: {
                "user_id": str,
                "client_id": str,
                "scope": str,
                "issued_at": int,
                "expires_at": int,
                "revoked": bool,
                "rotated_to": str (optional)
            }
        """
        try:
            file_data = self._read_with_lock(self.refresh_tokens_file)
            file_data["tokens"][token] = data
            self._write_with_lock(self.refresh_tokens_file, file_data)
            logger.log_info(f"Saved refresh-token: {token[:8]}...")
            return True
        except Exception as e:
            logger.log_error(f"Failed to save refresh-token: {e}")
            return False
    
    def get_refresh_token(self, token: str) -> Optional[dict]:
        """refresh-token 조회"""
        try:
            file_data = self._read_with_lock(self.refresh_tokens_file)
            return file_data["tokens"].get(token)
        except Exception as e:
            logger.log_error(f"Failed to get refresh-token: {e}")
            return None
    
    def revoke_refresh_token(self, token: str, rotated_to: Optional[str] = None) -> bool:
        """refresh-token 회수"""
        try:
            file_data = self._read_with_lock(self.refresh_tokens_file)
            if token in file_data["tokens"]:
                file_data["tokens"][token]["revoked"] = True
                if rotated_to:
                    file_data["tokens"][token]["rotated_to"] = rotated_to
                self._write_with_lock(self.refresh_tokens_file, file_data)
                logger.log_info(f"Revoked refresh-token: {token[:8]}...")
                return True
            return False
        except Exception as e:
            logger.log_error(f"Failed to revoke refresh-token: {e}")
            return False
    
    def cleanup_expired_tokens(self) -> int:
        """만료된 refresh-token 정리"""
        try:
            now = int(time.time())
            file_data = self._read_with_lock(self.refresh_tokens_file)
            
            expired_tokens = [
                token for token, data in file_data["tokens"].items()
                if data.get("expires_at", 0) < now or data.get("revoked", False)
            ]
            
            for token in expired_tokens:
                del file_data["tokens"][token]
            
            if expired_tokens:
                self._write_with_lock(self.refresh_tokens_file, file_data)
                logger.log_info(f"Cleaned up {len(expired_tokens)} expired/revoked refresh-tokens")
            
            return len(expired_tokens)
        except Exception as e:
            logger.log_error(f"Failed to cleanup refresh-tokens: {e}")
            return 0
