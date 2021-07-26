import json
import boto3
import logging
from datetime import datetime
from dateutil import tz

logger = logging.getLogger()
logger.setLevel(logging.INFO)

from_zone = tz.gettz('UTC')
to_zone = tz.gettz('Asia/Jerusalem')

client = boto3.client('iot-data', region_name='eu-west-1')


def lambda_handler(event, context):
    logger.info('Received event: ' + json.dumps(event))

    message = event['message']
    sms = event["sms"]

    logger.info(sms)

    timestamp = datetime.utcnow()
    timestamp = timestamp.replace(tzinfo=from_zone)
    timestamplocal = timestamp.astimezone(to_zone)
    timelocal = int(timestamplocal.strftime('%H'))

    sns = boto3.client('sns')
    sms_setup = sns.set_sms_attributes(
        attributes={
            'DefaultSenderID': 'PowerStat'
            }
    )
    message = "%s\n%s" % (message, timestamplocal.strftime('%d-%m-%Y %H:%M:%S'))
    sns.publish(PhoneNumber=sms, Message=message)
    logger.info('SMS ' + message + ' has been sent to ' + sms)

    return "OK"
    
