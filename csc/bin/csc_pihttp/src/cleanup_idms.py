"""
IDMS 데이터 정리 스크립트

만료되거나 회수된 auth-code 및 refresh-token을 정리합니다.
"""

import sys
import os

# 경로 추가
sys.path.insert(0, os.path.dirname(__file__))

from idms_storage import IdmsStorage

def main():
    storage = IdmsStorage()
    
    print("=" * 60)
    print("IDMS 데이터 정리 시작")
    print("=" * 60)
    
    # auth-code 정리
    print("\n[1] Auth-code 정리 중...")
    cleaned_codes = storage.cleanup_expired_codes()
    print(f"   ✅ {cleaned_codes}개의 만료된 auth-code 삭제")
    
    # refresh-token 정리
    print("\n[2] Refresh-token 정리 중...")
    cleaned_tokens = storage.cleanup_expired_tokens()
    print(f"   ✅ {cleaned_tokens}개의 만료/회수된 refresh-token 삭제")
    
    print("\n" + "=" * 60)
    print("정리 완료!")
    print("=" * 60)

if __name__ == "__main__":
    main()
