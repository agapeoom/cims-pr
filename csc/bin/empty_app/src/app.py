import argparse
import time
import traceback

from util.pi_http.http_server import HttpServer
from util.log_util import Logger


# for test
from util.pi_http.http_handler import HandlerArgs, HandlerResult
async def _rcv_msg(handler_args: HandlerArgs, kwargs: dict) -> HandlerResult:
    if handler_args.method != 'GET':
        return HandlerResult(status=405, body=f'invalid method')
    return HandlerResult(status=200, body='OK')
TEST_HANDLER_LIST = [("/test", _rcv_msg, {})]


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    # parser.add_argument('--agent-id', type=str, required=True, help='Agent ID')
    args_dict = vars(parser.parse_args())

    # set arguments

    # init logger
    logger = Logger(log_dir="../logs", log_file_prefix="app", retention_day=30)

    http_server = None
    try:
        logger.log_info(f'==================== start ====================')

        http_server = HttpServer("127.0.0.1", 8080)
        http_server.add_dynamic_rules(TEST_HANDLER_LIST)
        http_server.start()

        while True:
            time.sleep(1)

    except Exception as e:
        tb_str = traceback.format_exc()
        logger.log_error(f'==================== stop : {e} : {tb_str} ====================')
        if http_server: http_server.stop(5)
