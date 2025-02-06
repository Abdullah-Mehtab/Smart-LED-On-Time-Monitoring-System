function doGet(e) {
  Logger.log(e.parameter);

  // 1) If request has lednumber + ledstatus + datetime => log data
  if (e.parameter.lednumber && e.parameter.ledstatus && e.parameter.datetime) {
    return appendData(e);
  }

  // 2) If request has lednumber + date/time range => compute ON time
  else if (e.parameter.lednumber && e.parameter.startDate && e.parameter.startTime && 
           e.parameter.endDate && e.parameter.endTime) {
    return calculateOnTime(e); // <== returns JSON directly
  }

  // 3) Otherwise invalid
  else {
    return ContentService.createTextOutput("Invalid parameters provided.");
  }
}

function appendData(e) {
  var sheet_id = "1R7i7FmZVECeToWY2MPQPBVnUmgSYDv0y8x2fySPQb8U"; // Replace with your actual sheet ID
  var sheet_name = "EmbeddedProject"; // Replace with your actual sheet name
  var ss = SpreadsheetApp.openById(sheet_id);
  var sheet = ss.getSheetByName(sheet_name);

  // Extract parameters
  var ledNumber = e.parameter.lednumber;
  var ledStatus = e.parameter.ledstatus;
  var datetime = e.parameter.datetime; // Combined date and time
  
  if (!ledNumber || !ledStatus || !datetime) {
    return ContentService.createTextOutput("Missing required parameters.");
  }

  // Append row: [datetime, LED number, status]
  sheet.appendRow([datetime, ledNumber, ledStatus === "1" ? "ON" : "OFF"]);

  // Return success message
  return ContentService.createTextOutput("Data appended successfully.");
}

function calculateOnTime(e) {
  // Replace with your actual sheet ID and name
  var sheet_id = "1R7i7FmZVECeToWY2MPQPBVnUmgSYDv0y8x2fySPQb8U";
  var sheet_name = "EmbeddedProject";

  var ss = SpreadsheetApp.openById(sheet_id);
  var sheet = ss.getSheetByName(sheet_name);

  var ledNumber = e.parameter.lednumber;
  var startDate = e.parameter.startDate;
  var startTime = e.parameter.startTime;
  var endDate   = e.parameter.endDate;
  var endTime   = e.parameter.endTime;

  if (!ledNumber || !startDate || !startTime || !endDate || !endTime) {
    return ContentService.createTextOutput("Missing required parameters.");
  }

  var startDateTime = new Date(startDate + " " + startTime);
  var endDateTime   = new Date(endDate   + " " + endTime);

  var data          = sheet.getDataRange().getValues();
  var totalOnMillis = 0;

  for (var i = 1; i < data.length; i++) {
    var recordDate = new Date(data[i][0]);  // Column A: Datetime
    var recordLED  = data[i][1];           // Column B: LED number
    var recordStat = data[i][2];           // Column C: Status ("ON"/"OFF")

    if (String(recordLED) === String(ledNumber) &&
        recordDate >= startDateTime && recordDate <= endDateTime) {
      if (recordStat === "ON") {
        // Check next row or use end date/time
        var nextTime = endDateTime;
        if (i + 1 < data.length) {
          var nextDate  = new Date(data[i + 1][0]);
          var nextLED   = data[i + 1][1];
          var nextStat  = data[i + 1][2];
          if (String(nextLED) === String(ledNumber) && nextStat === "OFF") {
            nextTime = nextDate;
          }
        }
        totalOnMillis += (nextTime - recordDate);
      }
    }
  }

  var totalSecs = Math.floor(totalOnMillis / 1000);
  var hours     = Math.floor(totalSecs / 3600);
  var minutes   = Math.floor((totalSecs % 3600) / 60);
  var seconds   = Math.floor(totalSecs % 60);

  var result = {
    hours: Math.abs(hours),
    minutes: Math.abs(minutes),
    seconds: Math.abs(seconds)
  };

  // Directly return JSON. No redirection, no extra HTML:
  return ContentService
    .createTextOutput(JSON.stringify(result))
    .setMimeType(ContentService.MimeType.JSON);
}