/*
 *
 SW01 for HW01
 Emulator CD changer Becker BE 980, BE 982
 Tested:
  Becker Grand Prix 2000 BE 1319
  Becker Mexico 2000 BE 1431, BE 1432, BE 1436
  https://github.com/niz93/CDCEMU2000
 */




long previousMillis = 0;        // Время последней отправки
long previousMillisButton = 0;  // Время последнего нажатия кнопки
long previousMillisLed = 0;     // Время последнего выключения светодиода
long previousMillisTime = 0;    // Время в секундах
long interval = 1000;           // Интервал отправки в миллисекундах
long intervalLed = 5;           // Интервал работы светодиода
long intervalButton = 50;

#define RS485DE 2                   // Направление потока
#define SOUNDON 6                   // Включение звука
#define ADDR (byte)0x32             // Адрес приёмника
#define ADDR0 (byte)0x30            // Адрес приёмника
#define MASTER_ADDR (byte)0x11      // Адрес источника
#define MASTER_ADDR_OUT (byte)0x10  // Адрес получателя
#define TALK_STATUS (byte)0xFC      // Подверждение передачи
#define APPROVE_STATUS (byte)0x0F   // Подверждение приёма
#define MAX_DATA_SIZE 5             // Максимальная длина получаемого сообщения
#define MAX_TRACK (byte)0x99        // Максимальный номер трека
#define PlayBT 6                    // Пин воспроизведения
#define PauseBT 7                   // Пин паузы
#define SkipFBT 8                   // Пин переключения вперёд
#define SkipBBT 9                   // Пин переключения назад


byte MSG_OUT[12] = { TALK_STATUS, MASTER_ADDR_OUT, ADDR, 0x07, 0x2B, 0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00 };          //шаблон сообщения, дефолтно в паузе первого диска
byte MSG_Mag1CD[8] = { TALK_STATUS, MASTER_ADDR_OUT, ADDR, 0x03, 0x2C, 0x20, 0x01, 0xD3 };                                //Загружен 1 диск
byte MSG_CDInfo[12] = { TALK_STATUS, MASTER_ADDR_OUT, ADDR, 0x07, 0x2B, 0xA1, 0x01, MAX_TRACK, 0x66, 0x77, 0x88, 0x51 };  //Данные о треках на диске
byte MSG_Play1CD1TB[12] = { TALK_STATUS, MASTER_ADDR_OUT, ADDR, 0x07, 0x2B, 0xB1, 0x01, 0x00, 0x00, 0x00, 0x01, 0x40 };   //Смена состояний

bool flag = 0;
byte track = 0x01;
byte CRCa = 0xFF;
byte timeSec = 0x00;
byte timeMin = 0x00;
byte time01Sec = 0x00;
byte time10Sec = 0x00;
byte time01Min = 0x00;
byte time10Min = 0x00;

enum RecieveState {
  WAIT_ADDR = 0,
  WAIT_MASTER_ADDR,
  WAIT_DATA_SIZE,
  WAIT_DATA,
  VALIDATE
};

struct Packet {
  byte addr = 0;
  const byte masterAddr = MASTER_ADDR;
  byte dataLength = 0;
  byte data[MAX_DATA_SIZE];
};


byte rawData[MAX_DATA_SIZE];
byte waitedRawDataSize = 0;
byte dataCounter = 0;  // Счетчик приема поля data

RecieveState receiveState = RecieveState::WAIT_ADDR;

void processReceive(byte *data, int length);
void getPacket(Packet packet);

void setup() {

  pinMode(RS485DE, OUTPUT);
  digitalWrite(RS485DE, LOW);
  Serial.begin(4800, SERIAL_8E2);  // initialize both serial ports:

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SOUNDON, OUTPUT);
  pinMode(PlayBT, OUTPUT);
  pinMode(PauseBT, OUTPUT);
  pinMode(SkipFBT, OUTPUT);
  pinMode(SkipBBT, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(SOUNDON, LOW);
  digitalWrite(PlayBT, LOW);
  digitalWrite(PauseBT, LOW);
  digitalWrite(SkipFBT, LOW);
  digitalWrite(SkipBBT, LOW);
}

void loop() {

  if (millis() - previousMillisLed > intervalLed) {  // если время свечения светодиода вышло, выключаем
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (millis() - previousMillisButton > intervalButton) {  // если время нажатия кнопки, выключаем
    digitalWrite(PlayBT, LOW);
    digitalWrite(PauseBT, LOW);
    digitalWrite(SkipFBT, LOW);
    digitalWrite(SkipBBT, LOW);
  }

  //======================================================================================================================
  // Подсчёт проигранного времени и включение звука
  if (MSG_OUT[5] == 0x81 || MSG_OUT[5] == 0x41) {                          // Если идёт возспроизвдение
    digitalWrite(SOUNDON, HIGH);                     // Включить звук
    if (millis() - previousMillisTime > interval) {  // Если прошла секунда

      previousMillisTime = millis();  // Фиксируем время
      time01Sec = time01Sec + 0x01;   // Добавляем еденицу секунды
    }
    if (time01Sec > 0x09) {          // Если едениц секунд более 9
      time01Sec = 0x00;              // Скидываем еденицы секунд в 0
      time10Sec = time10Sec + 0x10;  // Добавляем десятку секунд
    }
    if (time10Sec > 0x50) {          // Если десяток секунд более 5
      time10Sec = 0x00;              // Скидываем десятки секунд в 0
      time01Min = time01Min + 0x01;  // Добавляем еденицу минут
    }
    timeSec = time01Sec + time10Sec;  // Складываем десятки и еденицы секунд

    if (time01Min > 0x09) {          // Если едениц минут более 9
      time01Min = 0x00;              // Скидываем еденицы минут в 0
      time10Min = time10Min + 0x10;  // Добавляем десятку секунд
    }
    if (time10Min > 0x50) {  // Если десяток минут более 5
      time10Min = 0x00;      // Скидываем десятки минут в 0
    }
    timeMin = time01Min + time10Min;  // Складываем десятки и еденицы минут

    MSG_OUT[7] = timeMin;   // Отправка минут в общее время
    MSG_OUT[8] = timeSec;   // Отправка секунд в общее время
    MSG_OUT[9] = timeMin;   // Отправка минут в время проигрывания трека
    MSG_OUT[10] = timeSec;  // Отправка минут в время проигрывания трека
  }

  if (MSG_OUT[5] == 0x01) {      // Если воспроизвдение остановлено
    digitalWrite(SOUNDON, LOW);  // Выключить звук
  }

  //======================================================================================================================
  // Смена состояния
  if (flag > 0) {
    digitalWrite(RS485DE, HIGH);

    for (byte i = 1; i < 11; i++) {
      CRCa = (CRCa ^ MSG_Play1CD1TB[i]);
      MSG_Play1CD1TB[11] = CRCa;
    }

    for (byte i = 0; i < 12; i++) {
      Serial.write(MSG_Play1CD1TB[i]);
    }
    Serial.flush();
    digitalWrite(RS485DE, LOW);
    CRCa = 0xFF;
    flag = 0;
    previousMillis = millis();
  }
  //======================================================================================================================
  // Отправка сообщения
  if (millis() - previousMillis > interval) {
    previousMillis = millis();
    digitalWrite(RS485DE, HIGH);

    for (byte i = 1; i < 11; i++) {
      CRCa = CRCa ^ MSG_OUT[i];
      MSG_OUT[11] = CRCa;
    }

    for (byte i = 0; i < 12; i++) {
      Serial.write(MSG_OUT[i]);
    }
    Serial.flush();
    digitalWrite(RS485DE, LOW);
    CRCa = 0xFF;
  }

  //=====================================================================================================================
  // Переполнение треков
  if (track == MAX_TRACK) {
    track = 0x01;
    MSG_Play1CD1TB[6] = track;
    MSG_OUT[6] = track;
    flag = 1;
  }
  //======================================================================================================================
  if (Serial.available() > 0) {
    byte currentByte = Serial.read();
    processReceive(&currentByte, 1);
  }
}

/**
 * @brief Прием сырых данных и парсинг
 */
void processReceive(byte *data, int length) {
  static Packet packet;

  for (int i = 0; i < length; i++) {
    byte currentByte = data[i];
    switch (receiveState) {
      //======================================================================================================================
      case RecieveState::WAIT_ADDR:
        {
          if (currentByte == ADDR0 || currentByte == ADDR) { 
            receiveState = RecieveState::WAIT_MASTER_ADDR;
            packet.addr = currentByte;
          }
          break;
        }
      //======================================================================================================================
      case RecieveState::WAIT_MASTER_ADDR:
        {
          if (currentByte == MASTER_ADDR) {
            receiveState = RecieveState::WAIT_DATA_SIZE;
          } else {
            receiveState = RecieveState::WAIT_ADDR;
          }
          break;
        }
      //======================================================================================================================
      case RecieveState::WAIT_DATA_SIZE:
        {
          waitedRawDataSize = currentByte;
          // Слишком большая длина пакета
          if (waitedRawDataSize > MAX_DATA_SIZE) {
            receiveState = RecieveState::WAIT_ADDR;
          }
          receiveState = RecieveState::WAIT_DATA;
          packet.dataLength = waitedRawDataSize;
          break;
        }
      //======================================================================================================================
      case RecieveState::WAIT_DATA:
        {
          packet.data[dataCounter] = currentByte;
          dataCounter++;
          if (waitedRawDataSize == dataCounter) {
            receiveState = RecieveState::VALIDATE;
            dataCounter = 0;
          }
          break;
        }
      //======================================================================================================================
      case RecieveState::VALIDATE:
        {
          byte crc = currentByte;  // Пришедшая CRC

          byte CRC = 0xFF ^ packet.addr;  // Расчёт CRC
          CRC = CRC ^ MASTER_ADDR;
          CRC = CRC ^ packet.dataLength;

          for (byte i = 0; i < packet.dataLength; i++) {
            CRC = CRC ^ packet.data[i];
          }
          bool isCRCOK = CRC == crc;
          if (isCRCOK) {
            digitalWrite(RS485DE, HIGH);            // Режим передачи
            Serial.write(APPROVE_STATUS);           // Подтверждаем приём
            Serial.flush();                         // Ждём окончание передачи
            digitalWrite(RS485DE, LOW);             // Режим приёма
            packet.dataLength = waitedRawDataSize;  // Сохраняем длину пакета
            getPacket(packet);                      // Сохраняем пакет
          }
          receiveState = RecieveState::WAIT_ADDR;  // Сброс состояния
          break;
        }
      default:
        break;
    }
  }
}

/*
 * @brief Обработка готового пакета 
 */
void getPacket(Packet packet) {
  digitalWrite(LED_BUILTIN, HIGH);  // сигнализируем о принятом пакете
  previousMillisLed = millis();

  //======================================================================================================================
  if (packet.dataLength == 0x02) {                           // когда на входе 2 байта дата
    if (packet.data[0] == 0x62 && packet.data[1] == 0x03) {  // Open CDC MODE

      MSG_OUT[5] = 0x81;
      flag = 1;
      digitalWrite(PlayBT, HIGH);
      previousMillisButton = millis();
    }
    if (packet.data[0] == 0x62 && packet.data[1] == 0x0A) {  // Close CDC MODE
      MSG_OUT[5] = 0x01;
      flag = 1;
      digitalWrite(PauseBT, HIGH);
      previousMillisButton = millis();
    }
    if (packet.data[0] == 0x62 && packet.data[1] == 0x0E) {  // Play normal
      MSG_OUT[5] = 0x81;
      digitalWrite(PlayBT, HIGH);
      previousMillisButton = millis();
    }
    if (packet.data[0] == 0x62 && packet.data[1] == 0x0B) {  // Play random
      MSG_OUT[5] = 0x41;
      digitalWrite(PauseBT, HIGH);
      previousMillisButton = millis();
    }
  }
  //======================================================================================================================
  if (packet.dataLength == 0x03) {                                                                                 // когда на входе 3 байта дата
    if (packet.data[0] == 0x62 && packet.data[1] == 0x0C && (packet.data[2] == 0x07 || packet.data[2] == 0x08)) {  //Сколько дисков загружено?


      previousMillis = millis();
      digitalWrite(RS485DE, HIGH);
      for (byte i = 0; i < 8; i++) {
        Serial.write(MSG_Mag1CD[i]);  // Передача данных о загруженных дисках
      }
      Serial.flush();
      digitalWrite(RS485DE, LOW);

      delay(5);

      digitalWrite(RS485DE, HIGH);
      for (byte i = 0; i < 12; i++) {
        Serial.write(MSG_CDInfo[i]);  // Передача данных о загруженном диске
      }
      Serial.flush();
      digitalWrite(RS485DE, LOW);

      flag = 1;
    }

    if (packet.data[0] == 0x62 && packet.data[1] == 0x0C && packet.data[2] == 0x08) {  //Сколько дисков загружено?


      previousMillis = millis();
      digitalWrite(RS485DE, HIGH);
      for (byte i = 0; i < 8; i++) {
        Serial.write(MSG_Mag1CD[i]);  // Передача данных о загруженных дисках
      }
      Serial.flush();
      digitalWrite(RS485DE, LOW);

      delay(5);

      digitalWrite(RS485DE, HIGH);
      for (byte i = 0; i < 12; i++) {
        Serial.write(MSG_CDInfo[i]);  // Передача данных о загруженном диске
      }
      Serial.flush();
      digitalWrite(RS485DE, LOW);

      flag = 1;
    }
  }
  //======================================================================================================================
  if (packet.dataLength == 0x04) {                           // Когда на входе 4 байта дата
    if (packet.data[0] == 0x62 && packet.data[1] == 0x11) {  // Запроса на переключение трека

      byte requestedTrack = packet.data[3];  // запрашиваемый номер трека

      if ((requestedTrack - track >= 1 && requestedTrack - track < 50) || requestedTrack - track < -50) {  // если следующий трек больше предыдушего или если переходим от последнего к первому
        digitalWrite(SkipFBT, HIGH);
        previousMillisButton = millis();
        time01Sec = 0x00;  // Обнуляем таймеры
        time10Sec = 0x00;
        time01Min = 0x00;
        time10Min = 0x00;
      }
      if ((requestedTrack - track <= -1 && requestedTrack - track > -50) || requestedTrack - track > 50) {  //если следующий трек меньше предыдущего или если переходим от первого к последнему
        digitalWrite(SkipBBT, HIGH);
        previousMillisButton = millis();
        time01Sec = 0x00;  // Обнуляем таймеры
        time10Sec = 0x00;
        time01Min = 0x00;
        time10Min = 0x00;
      }



      if (requestedTrack - track == 0) {  //если запрашивается тот же самый трек
        if (time01Sec > 4 || time10Sec > 0) { // защита от перескоков
          digitalWrite(SkipBBT, HIGH);
          previousMillisButton = millis();
          time01Sec = 0x00;  // Обнуляем таймеры
          time10Sec = 0x00;
          time01Min = 0x00;
          time10Min = 0x00;
        }
      }

      track = requestedTrack;  //отправка номера трека в работу

      MSG_Play1CD1TB[6] = track;
      MSG_OUT[6] = track;
      flag = 1;
    }
  }
  //======================================================================================================================
}
