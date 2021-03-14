enum gameMode
{
  INSERT_COIN,
  SEND_SIGNAL,
  WAIT_RESPONSE_SIGNAL,
  CLEAR_STATUS,
  SETUP_GAME,
  CHOOSE_COLOR,
  CHECK_COLOR,
  COMPUTE_ANSWER,
  SHOW_ANSWER
};

enum actionToMake
{
  NOTHING,
  COUNT,
  DEPLOY_SOLUTION,
  CHECK_SOLUTION,
};

#define LENGTH_DATA 12
#define NB_COLORS 6
#define PULSE_LENGTH 3000
#define COLOR_SIGNAL makeColorHSB(160, 255, 64)
#define MAX_WAITING 500

int waitingCounter;
byte waitingFace;

byte actualColor;

byte blinkState = INSERT_COIN;
byte whoIAm;
byte fromSide;
byte toSide;
byte nbBlinks;

Timer timer;

byte data[LENGTH_DATA + 4];
byte solution[LENGTH_DATA];
// LENGTH_DATA : First Sender
// LENGTH_DATA + 1 : Next gameMode
// LENGTH_DATA + 2 : Counter
// LENGTH_DATA + 3 : Current actionToMake

byte colorChoosed;
byte colorsHue[NB_COLORS] = {22, 49, 82, 99, 160, 200}; // Orange, Yellow, Green, Cyan, Blue, Purple

void setup()
{
  randomize();
  setValueSentOnAllFaces(0);
  actualColor = random(255);
}

void loop()
{
  if(timer.isExpired())
  {
//    timer.set(100);
    switch(blinkState)
    {
      case INSERT_COIN:
        // Wait for a double click
        insertCoin();
      break;
      case SEND_SIGNAL:
        sendSignal();
      break;
      case WAIT_RESPONSE_SIGNAL:
        waitResponseSignal();
      break;
      case SETUP_GAME:
        setupGame();
      break;
      case CHOOSE_COLOR:
        chooseColor();
      break;
      case CHECK_COLOR:
        checkColor();
      break;
      case COMPUTE_ANSWER:
        computeAnswer();
      break;
      case SHOW_ANSWER:
        showAnswer();
      break;
      case CLEAR_STATUS:
        clearStatus();
      break;
    }
  }
}

void insertCoin()
{
  setColor(OFF);
  int pulseProgress = millis() % PULSE_LENGTH;
  byte pulseMapped = map(pulseProgress, 0, PULSE_LENGTH, 0, 255);
  byte dimness = sin8_C(pulseMapped);
  pulseMapped = map(dimness, 0, 255, 0, 192);
  if(pulseMapped == 0)
      actualColor = random(255);

  setColorOnFace(makeColorHSB(actualColor, 255, pulseMapped), (millis() / 300) % 6);
  setColorOnFace(makeColorHSB(actualColor, 255, pulseMapped), ((millis() / 300) + 1) % 6 );
  setColorOnFace(makeColorHSB(actualColor, 255, pulseMapped), ((millis() / 300) + 2) % 6 );
  setColorOnFace(makeColorHSB(actualColor, 255, pulseMapped), ((millis() / 300) + 3) % 6 );
  setColorOnFace(makeColorHSB(actualColor, 255, pulseMapped), ((millis() / 300) + 4) % 6 );
  if(buttonSingleClicked() || buttonDoubleClicked() || buttonMultiClicked())
  {
    whoIAm = 0;
    data[LENGTH_DATA] = whoIAm; // IDFrom
    data[LENGTH_DATA + 1] = SETUP_GAME; // Next gameMode
    data[LENGTH_DATA + 2] = 0; // Counter
    data[LENGTH_DATA + 3] = COUNT; // Order
    fromSide = 0;
    toSide = 0;
    setValueSentOnAllFaces(63);
    blinkState = SEND_SIGNAL;
  }
  else
  {
    waitForSignal();
  }
}

void waitForSignal()
{
  FOREACH_FACE(f)
  {
    if(isDatagramReadyOnFace(f))
    {
      const byte *datagramPayload = getDatagramOnFace(f);
      if(getDatagramLengthOnFace(f) == sizeof(data))
      {
        markDatagramReadOnFace(f);
        for(byte n = 0; n < sizeof(data); n++)
        {
          data[n] = datagramPayload[n];
        }
        fromSide = f;
        toSide = (fromSide + 1) % FACE_COUNT;
        setValueSentOnAllFaces(63);
        blinkState = SEND_SIGNAL;
        setColorOnFace(COLOR_SIGNAL, fromSide);
        switch(data[LENGTH_DATA + 3])
        {
          case COUNT:
            whoIAm = data[LENGTH_DATA + 2] + 1;
            colorChoosed = 0;
          break;
          case DEPLOY_SOLUTION:
            for(byte n = 0; n < LENGTH_DATA; n++)
            {
              solution[n] = data[n];
            }
          break;
          case CHECK_SOLUTION:
            data[whoIAm] = colorChoosed;
          break;
        }
        data[LENGTH_DATA + 2] = data[LENGTH_DATA + 2] + 1;
      }
    }
  }
}

void sendSignal()
{
  setColorOnFace(COLOR_SIGNAL, toSide);
  if(!isValueReceivedOnFaceExpired(toSide))
  {
    byte value = getLastValueReceivedOnFace(toSide);
    if(value == 0)
    {
      waitingCounter = 0;
      waitingFace = toSide;
      sendDatagramOnFace(&data , sizeof(data), toSide);
      blinkState = WAIT_RESPONSE_SIGNAL;
      return;
    }
  }
  toSide = (toSide + 1) % FACE_COUNT;
  if(fromSide == toSide)
  {
    setColor(OFF);
    if(whoIAm == data[LENGTH_DATA])
    {
      // Transmission terminé !
      nbBlinks = data[LENGTH_DATA + 2] + 1;
    }
    else
    {
      waitingCounter = 0;
      waitingFace = fromSide;
      sendDatagramOnFace(&data , sizeof(data), fromSide);
    }

    blinkState = CLEAR_STATUS;
  }
}

void waitResponseSignal()
{
  waitingCounter++;
  if(waitingCounter > MAX_WAITING)
  {
      waitingCounter = 0;
      sendDatagramOnFace(&data , sizeof(data), waitingFace);
  }
  if(isDatagramReadyOnFace(toSide))
  {
    const byte *datagramPayload = getDatagramOnFace(toSide);
    if(getDatagramLengthOnFace(toSide) == sizeof(data))
    {
      markDatagramReadOnFace(toSide);
      for(byte n = 0; n < sizeof(data); n++)
      {
        data[n] = datagramPayload[n];
      }
      blinkState = SEND_SIGNAL;
    }
  }
}

void clearStatus()
{
  if(whoIAm == data[LENGTH_DATA])
  {
    setValueSentOnAllFaces(0);
    FOREACH_FACE(f)
    {
      if(!isValueReceivedOnFaceExpired(f))
      {
        byte value = getLastValueReceivedOnFace(f);
        if(value != 0)
          return;
      }
    }
    blinkState = data[LENGTH_DATA + 1];
    timer.set(200);
  }
  else
  {
    if(!isValueReceivedOnFaceExpired(fromSide))
    {
      byte value = getLastValueReceivedOnFace(fromSide);
      if(value != 0)
      {
        return;
      }
      setValueSentOnAllFaces(0);
      FOREACH_FACE(f)
      {
        if(!isValueReceivedOnFaceExpired(f))
        {
          byte value = getLastValueReceivedOnFace(f);
          if(value != 0)
            return;
        }
      }
      blinkState = data[LENGTH_DATA + 1];
      timer.set(200);
    }
  }
}

void setupGame()
{
  if(whoIAm == 0)
  {
    whoIAm = 0;
    data[LENGTH_DATA] = 0; // IDFrom
    data[LENGTH_DATA + 1] = CHOOSE_COLOR; // Next gameMode
    data[LENGTH_DATA + 2] = 0; // Counter
    data[LENGTH_DATA + 3] = DEPLOY_SOLUTION; // Order
    fromSide = 0;
    toSide = 0;
    setValueSentOnAllFaces(63);
    blinkState = SEND_SIGNAL;

    for(byte n = 0; n < LENGTH_DATA; n++)
    {
      if(n < nbBlinks)
      {
        data[n] = random(NB_COLORS - 1);
      }
      else
      {
        data[n] = 0;
      }
      solution[n] = data[n];
    }
  }
  else
  {
    waitForSignal();
  }
}

void chooseColor()
{
  data[whoIAm] = colorChoosed;
  setColor(makeColorHSB(colorsHue[colorChoosed], 255, 255));
  int pulseProgress = millis() % PULSE_LENGTH;
  byte pulseMapped = map(pulseProgress, 0, PULSE_LENGTH, 0, 255);
  byte dimness = sin8_C(pulseMapped);
  pulseMapped = map(dimness, 0, 255, 0, 128);
  setColorOnFace(makeColorHSB(colorsHue[colorChoosed], 255, pulseMapped), colorChoosed);
  if(buttonSingleClicked() || buttonDoubleClicked())
  {
    colorChoosed = (colorChoosed + 1) % NB_COLORS;
    return;
  }
  if(buttonLongPressed())
  {
    blinkState = CHECK_COLOR;
    return;
  }
  if(buttonMultiClicked())
  {
    if(buttonClickCount() == 5)
    {
      setColor(makeColorHSB(colorsHue[solution[whoIAm]], 255, 255));
      setColorOnFace(OFF, solution[whoIAm]);
      timer.set(2000);
    }
    else
    {
      // Reset !
      //reset();
      return;
    }
  }
  waitForSignal();
}

void reset()
{
    whoIAm = 0;
    data[LENGTH_DATA] = whoIAm; // IDFrom
    data[LENGTH_DATA + 1] = INSERT_COIN; // Next gameMode
    data[LENGTH_DATA + 2] = 0; // Counter
    data[LENGTH_DATA + 3] = COUNT; // Order
    fromSide = 0;
    toSide = 0;
    setValueSentOnAllFaces(63);
    blinkState = SEND_SIGNAL;
    colorChoosed = 0;
}

void checkColor()
{
    data[LENGTH_DATA] = whoIAm; // IDFrom
    data[LENGTH_DATA + 1] = COMPUTE_ANSWER; // Next gameMode
    data[LENGTH_DATA + 2] = 0; // Counter
    data[LENGTH_DATA + 3] = CHECK_SOLUTION; // Order
    fromSide = 0;
    toSide = 0;
    setValueSentOnAllFaces(63);
    blinkState = SEND_SIGNAL;
}

void computeAnswer()
{
  if(whoIAm == data[LENGTH_DATA])
  {
    // Check proposal
    bool alreadyUseSol[LENGTH_DATA];
    bool alreadyUseProp[LENGTH_DATA];
    byte nbCorrect = 0;
    byte nbMisplaced = 0;
    for(byte n = 0; n < nbBlinks; n++)
    {
      alreadyUseSol[n] = false;
      alreadyUseProp[n] = false;
    }

    // Check des corrects
    for(byte idSolution = 0; idSolution < nbBlinks; idSolution++)
    {
      if(solution[idSolution] == data[idSolution])
      {
        nbCorrect++;
        alreadyUseSol[idSolution] = true;
        alreadyUseProp[idSolution] = true;
      }
    }

    // Check des mal placés
    for(byte idSolution = 0; idSolution < nbBlinks; idSolution++)
    {
      for(byte idProposal = 0; idProposal < nbBlinks; idProposal++)
      {
        if(idSolution == idProposal || alreadyUseSol[idSolution] || alreadyUseProp[idProposal])
          continue;
        if(solution[idSolution] == data[idProposal])
        {
          nbMisplaced++;
          alreadyUseSol[idSolution] = true;
          alreadyUseProp[idProposal] = true;
        }
      }
    }

    // Création de la réponse
    for(byte n = 0; n < nbBlinks; n++)
    {
      if(nbCorrect > 0)
      {
        data[n] = 2; // Correct
        nbCorrect--;
      }
      else
      {
        if(nbMisplaced > 0)
        {
          data[n] = 1; // Mal placé
          nbMisplaced--;
        }
        else
        {
          data[n] = 0; // Incorrect
        }
      }
    }

    data[LENGTH_DATA] = whoIAm; // IDFrom
    data[LENGTH_DATA + 1] = SHOW_ANSWER; // Next gameMode
    data[LENGTH_DATA + 2] = 0; // Counter
    data[LENGTH_DATA + 3] = NOTHING; // Order
    fromSide = 0;
    toSide = 0;
    setValueSentOnAllFaces(63);
    blinkState = SEND_SIGNAL;
  }
  else
  {
    waitForSignal();
  }
}

void showAnswer()
{
  switch(data[whoIAm])
  {
    case 0:
      setColor(OFF);
      setColorOnFace(makeColorHSB(0, 255, 128), (millis() / 300) % 6);
    break;
    case 1:
      setColor(RED);
      setColorOnFace(makeColorHSB(0, 255, 128), (millis() / 300) % 6);
    break;
    case 2:
      setColor(WHITE);
      setColorOnFace(makeColorHSB(0, 0, 128), (millis() / 300) % 6);
    break;
  }
  if(buttonLongPressed())
  {
    reset();
    return;
  }
  if(buttonSingleClicked() || buttonDoubleClicked() || buttonMultiClicked())
  {
    data[LENGTH_DATA] = whoIAm; // IDFrom
    data[LENGTH_DATA + 1] = CHOOSE_COLOR; // Next gameMode
    data[LENGTH_DATA + 2] = 0; // Counter
    data[LENGTH_DATA + 3] = DEPLOY_SOLUTION; // Order
    fromSide = 0;
    toSide = 0;
    setValueSentOnAllFaces(63);
    blinkState = SEND_SIGNAL;

    for(byte n = 0; n < LENGTH_DATA; n++)
    {
      data[n] = solution[n];
    }
  }
  else
  {
    waitForSignal();
  }
}
