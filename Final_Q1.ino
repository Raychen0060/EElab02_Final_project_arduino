const int pin[8] = {13,12,11,10,9,8,7,6};
const int left = 5;
const int right = 4;
const int button_pin = 1;
const int speaker = A1;
const int Trig_Pins = 3; // trig 腳位
const int Echo_Pins = 2; // echo 腳位

int k = 1;
int frame_period = 300; // 每個畫面的停留時間(ms)，跑馬燈速度

// --- Debounce code 所需的全域變數 ---
// INPUT_PULLUP 預設為 HIGH，按下為 LOW
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// --- 倒車警鈴 (Buzzer) 所需的全域變數 ---
int beepInterval = -1;           // 紀錄目前的靜音間隔，-1 代表完全靜音
unsigned long lastBuzzerTime = 0;// 紀錄上次切換喇叭狀態的時間
bool isBeeping = false;          // 紀錄現在喇叭是否正在叫

void setup(){
  for(int i=0;i<8;++i) pinMode(pin[i],OUTPUT);
  pinMode(left, OUTPUT);
  pinMode(right, OUTPUT);
  pinMode(button_pin, INPUT_PULLUP);
  pinMode(Trig_Pins, OUTPUT);
  pinMode(Echo_Pins, INPUT);
  pinMode(speaker, OUTPUT); // 設定喇叭腳位為輸出
  Serial.begin(9600);
}

void display_seven(bool lr, int num){
  if (lr){
    digitalWrite(left, LOW);
    digitalWrite(right, HIGH);
    shownum(num);
    delay(1);
  }
  else{
    digitalWrite(left, HIGH);
    digitalWrite(right, LOW);
    shownum(num);
    delay(1);
  }
}
// ==========================================
// 非阻擋式的喇叭更新函式 (用 millis 控制)
// ==========================================
void updateBuzzer() {
  // 如果設定為 -1，確保喇叭關閉並離開
  if (beepInterval == -1) {
    noTone(speaker);
    isBeeping = false;
    return;
  }
  unsigned long currentMillis = millis();
  if (isBeeping) {
    // 如果正在叫，檢查是否叫滿了 50ms
    if (currentMillis - lastBuzzerTime >= 50) {
      noTone(speaker);
      isBeeping = false;
      lastBuzzerTime = currentMillis;
    }
  } else {
    // 如果正在靜音，檢查靜音間隔是否結束，該發出下一聲了
    if (currentMillis - lastBuzzerTime >= beepInterval) {
      tone(speaker, 1000);
      isBeeping = true;
      lastBuzzerTime = currentMillis;
    }
  }
}

// ==========================================
// 按鈕偵測函式
// true 代表按鈕被按下，需要跳出當前模式
// ==========================================
bool showFrameAndCheckButton(int left_char, int right_char) {
  for (int i = 0; i < frame_period; i++) {
    // --- 檢查並更新喇叭狀態 ---
    updateBuzzer();
    
    // --- Debounce ---
    int reading = digitalRead(button_pin);
    if (reading != lastButtonState) lastDebounceTime = millis();
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;
        // 只有當新的穩定狀態是 LOW (代表按鈕被按下) 時才觸發切換
        if (buttonState == LOW) {
          // 切換模式時，立刻強制關閉喇叭，避免切到溫度模式時喇叭還在叫
          beepInterval = -1; 
          updateBuzzer();
          
          k = -k; // 切換模式
          shownum(10); // 清空畫面
          delay(frame_period);
          lastButtonState = reading; 
          return true; 
        }
      }
    }
    lastButtonState = reading;
    // --- Debounce End ---
    
    // 正常顯示這一個 Frame
    display_seven(1, left_char);
    display_seven(0, right_char);
  }
  return false; 
}

// ==========================================
// 溫度顯示模式
// ==========================================
void displayTemperature() {
  beepInterval = -1; // 確保在溫度模式時，警鈴是靜音的
  // 讀取並計算溫度
  float v = analogRead(A0);
  float t = v * (5 / 1024.0) * 1000 / 10.0;
  
  int t_int = (int)t;
  int t_dec = (t - t_int) * 10; // 取小數第一位

  // 定義跑馬燈的字元序列
  int seq[] = {
    t_int / 10,  // 十位數
    t_int % 10,  // 個位數
    '.',         // 字元 .
    t_dec,       // 小數第一位
    'o',         // 字元 o
    'c',         // 字元 c
    10           // 空白
  };
  int seq_len = sizeof(seq) / sizeof(seq[0]); // 字元序列長度
  // 利用 Sliding Window 依序顯示兩個字元
  for (int i = 0; i < seq_len - 1; i++) {
    if (showFrameAndCheckButton(seq[i], seq[i + 1])) return; // 如果 true，立刻提早結束這個函式
  }
  if (showFrameAndCheckButton(seq[seq_len - 1], seq[0])) return;
}

// ==========================================
// 超音波距離顯示模式
// ==========================================
void displayDistance() {
  // 觸發超音波感測器
  long duration;
  digitalWrite(Trig_Pins, HIGH);
  delayMicroseconds(10); //給予trig 10us TTL pulse,讓模組發射聲波
  digitalWrite(Trig_Pins, LOW);
  duration = pulseIn(Echo_Pins, HIGH, 30000); // 紀錄echo電位從high到low的時間，就是超音波來回的時間 30ms (約 5 公尺)
  int distance = duration * 0.034 / 2;

  // --- 根據距離設定喇叭的警報間隔 ---
  if (distance > 20 || distance == 0) { // 加了 == 0 預防超過 30000ms 的未回傳情況
    beepInterval = -1; 
  } else if (distance > 15) {
    beepInterval = 500;
  } else if (distance > 10) {
    beepInterval = 200;
  } else if (distance > 5) {
    beepInterval = 100;
  } else {
    beepInterval = 50;
  }

  // 定義跑馬燈的字元序列
  int seq[] = {
    (distance / 100) % 10, // 百位數
    (distance / 10) % 10,  // 十位數
    distance % 10,         // 個位數
    'c',                   // 字元 c
    'm',                   // 字元 m
    10                     // 空白
  };
  int seq_len = sizeof(seq) / sizeof(seq[0]); // 字元序列長度

  // 同樣依序推播畫面
  for (int i = 0; i < seq_len - 1; i++){
    if (showFrameAndCheckButton(seq[i], seq[i + 1])) return; // 按鈕被按下，提早結束
  }
  if (showFrameAndCheckButton(seq[seq_len - 1], seq[0])) return;
}

void loop() {
  if (k == 1) displayTemperature();
  else displayDistance();
}

void showSevenSeg(byte A, byte B, byte C, byte D, byte E, byte F, byte G, byte P){
  digitalWrite(pin[0], A);
  digitalWrite(pin[1], B);
  digitalWrite(pin[2], C);
  digitalWrite(pin[3], D);
  digitalWrite(pin[4], E);
  digitalWrite(pin[5], F);
  digitalWrite(pin[6], G);
  digitalWrite(pin[7], P);
}

void shownum(int num){
  switch(num){
    case 0:  showSevenSeg(0,0,0,0,0,0,1,1); delay(1); break;
    case 1:  showSevenSeg(1,0,0,1,1,1,1,1); delay(1); break;
    case 2:  showSevenSeg(0,0,1,0,0,1,0,1); delay(1); break;
    case 3:  showSevenSeg(0,0,0,0,1,1,0,1); delay(1); break;
    case 4:  showSevenSeg(1,0,0,1,1,0,0,1); delay(1); break;
    case 5:  showSevenSeg(0,1,0,0,1,0,0,1); delay(1); break;
    case 6:  showSevenSeg(0,1,0,0,0,0,0,1); delay(1); break;
    case 7:  showSevenSeg(0,0,0,1,1,1,1,1); delay(1); break;
    case 8:  showSevenSeg(0,0,0,0,0,0,0,1); delay(1); break;
    case 9:  showSevenSeg(0,0,0,0,1,0,0,1); delay(1); break;
    case 10: showSevenSeg(1,1,1,1,1,1,1,1); delay(1); break;
    
    case '.': showSevenSeg(1,1,1,1,1,1,1,0); delay(1); break;
    case 'o': showSevenSeg(0,0,1,1,1,0,0,1); delay(1); break;
    case 'c': showSevenSeg(0,1,1,0,0,0,1,1); delay(1); break;
    case 'm': showSevenSeg(1,1,0,1,0,1,0,1); delay(1); break; 
  }
}
