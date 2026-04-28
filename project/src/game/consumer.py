from channels.generic.websocket import WebsocketConsumer
from asgiref.sync import async_to_sync
import json

class GameConsumer(WebsocketConsumer):
    def connect(self):
        self.room_name = self.scope["url_route"]["kwargs"]["room_name"]
        async_to_sync(self.channel_layer.group_add)(
            self.room_name, self.channel_name
        )
        self.accept()

    def disconnect(self, close_code):
        async_to_sync(self.channel_layer.group_discard)(
            self.room_name, self.channel_name
        )

    def receive(self, text_data):
        data = json.loads(text_data)
        name = data['name']
        score = data['score']
        attention = data['attention']
        print(f"Received data: name={name}, score={score}, attention={attention}")
        message = {
            'name': name,
            'score': score,
            'attention': attention
        }
        async_to_sync(self.channel_layer.group_send)(
            self.room_name,
            {
                'type': 'game_message',
                'message': message
            }
        )
    
    def game_message(self, event):
        message = event['message']
        self.send(text_data=json.dumps(message))