from django.db import models

# Create your models here.
class Player(models.Model):
    name = models.CharField(max_length=100)
    score = models.IntegerField(default=0)
    attention = models.FloatField(default=0.0)

    def __str__(self):
        return self.name