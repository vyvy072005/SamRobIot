package com.example.samrobiot

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Button
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.samrobiot.ui.theme.SamRobIotTheme
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.eclipse.paho.client.mqttv3.MqttClient
import org.eclipse.paho.client.mqttv3.MqttConnectOptions
import org.eclipse.paho.client.mqttv3.MqttException
import org.eclipse.paho.client.mqttv3.MqttMessage

class MainActivity : ComponentActivity() {
    private lateinit var mqttClient: MqttClient

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        // Инициализация MQTT клиента
        val brokerUri = "tcp://10.207.145.117:1883"

        val clientId = "b46866ffa6c649429716cd75c2e7fkva"
        mqttClient = MqttClient(brokerUri, clientId, null)

        val options = MqttConnectOptions().apply {
            isCleanSession = true

        }

        // Подключение к брокеру с помощью корутин
        CoroutineScope(Dispatchers.IO).launch {
            try {
                mqttClient.connect(options)
                // Если подключение успешно
                println("Подключение успешно")
            } catch (e: MqttException) {
                println("Ошибка при подключении:")
                println("Reason code: ${e.reasonCode}")
                println("Message: ${e.message}")
                println("Localized message: ${e.localizedMessage}")
                println("Cause: ${e.cause}")
                e.printStackTrace()
            } catch (e: Exception) {
                println("Произошла непредвиденная ошибка:")
                e.printStackTrace()
            }
        }

        setContent {
            SamRobIotTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    GreetingWithButton(
                        name = "Android",
                        modifier = Modifier.padding(innerPadding),
                        sendCommand = { command, onSuccess ->
                            sendMotorCommand(command, onSuccess)
                        }
                    )
                }
            }
        }
    }



    fun sendMotorCommand(command: String, onResult: (Boolean) -> Unit) {
        val topic = "iot-2/evt/distance/"
        val message = MqttMessage(command.toByteArray())

        CoroutineScope(Dispatchers.IO).launch {
            try {
                if (mqttClient.isConnected) {
                    mqttClient.publish(topic, message)
                    // После успешной публикации возвращаем true
                    // Обновление UI должно быть на главном потоке
                    launch(Dispatchers.Main) {
                        onResult(true)
                    }
                } else {
                    // Клиент не подключен
                    launch(Dispatchers.Main) {
                        onResult(false)
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
                // В случае ошибки
                launch(Dispatchers.Main) {
                    onResult(false)
                }
            }
        }
    }




    private fun sendMessage() {
        val topic = "iot-2/evt/distance/"
        val payload = "1"
        val message = MqttMessage(payload.toByteArray())

        CoroutineScope(Dispatchers.IO).launch {
            try {
                if (mqttClient.isConnected) {
                    mqttClient.publish(topic, message)
                } else {

                }
            } catch (e: Exception) {
                e.printStackTrace()

            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (this::mqttClient.isInitialized && mqttClient.isConnected) {
            mqttClient.disconnect()
        }
    }
}

@Composable
fun GreetingWithButton(
    name: String,
    modifier: Modifier = Modifier,
    sendCommand: (String, (Boolean) -> Unit) -> Unit
) {
    var isMotorRunning by remember { mutableStateOf(false) }
    var isProcessing by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        //Text(text = "Hello $name!")

        Spacer(modifier = Modifier.height(16.dp))

        val buttonText = if (isMotorRunning) "Остановить" else "Старт мотора"

        Button(
            onClick = {
                if (isProcessing) return@Button // предотвращение повторных нажатий
                val command = if (isMotorRunning) "0" else "1"
                isProcessing = true
                sendCommand(command) { success ->
                    if (success) {
                        isMotorRunning = !isMotorRunning
                    }
                    isProcessing = false
                }
            }
        ) {
            Text(buttonText)
        }
    }
}
