' Rob Latour, Copyright 2021
'
' License: MIT
'
' uses the following two NuGet packages:
'   M2Mqtt by Paolo Patienno v4.3.0
'   SSH.net by Renci v2020.0.1

' Settings are manually configured in the file WeatherStationService.exe.config
'

Imports Renci.SshNet
Imports uPLibrary.Networking.M2Mqtt
Imports uPLibrary.Networking.M2Mqtt.Messages

Imports System.Text
Imports System.IO

Public Class Service1

    Friend client As MqttClient

    Private gProgramData_location As String
    Private gProgramData_filename As String

    Private DateAndTimeOfLastReading As DateTime
    Private Temperature As String
    Private Humidity As String
    Private Pressure As String

    Private NewWebPage As String = String.Empty

    Private Timer1 As System.Timers.Timer
    Private Timer2 As System.Timers.Timer
    Protected Overrides Sub OnStart(ByVal args() As String)

        'System.Threading.Thread.Sleep(30 * 1000)  'if debuging, this line may be uncommented to add in an initial delay to give you time to attach a process

        gProgramData_location = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData) & "\WeatherStation\"
        gProgramData_filename = gProgramData_location & "log.csv"

        If System.IO.Directory.Exists(gProgramData_location) Then
        Else
            System.IO.Directory.CreateDirectory(gProgramData_location)
        End If

        If System.IO.File.Exists(gProgramData_filename) Then
        Else
            IO.File.WriteAllText(gProgramData_filename, My.Resources.EmptyLog.ToString)
        End If

        WindowsToLinuxFileTransfer(My.Settings.ApcheServer_IPAddress, My.Settings.ApcheServer_UserID, My.Settings.ApcheServer_Password, My.Settings.ApcheServer_Webpage_Filename, My.Resources.UpdateComingSoonHTML.ToString)

        DateAndTimeOfLastReading = Now

        'if the webpage has not been updated after a specified time period then update it to show that it is out of date
        Timer1 = New System.Timers.Timer
        Timer1.Interval = (My.Settings.NewReadingsShouldBeAvailableInThisManyMinutes + 1) * 60 * 1000
        Timer1.Enabled = True
        AddHandler Timer1.Elapsed, AddressOf Timer1_Tick
        Timer1.Start()
        GC.KeepAlive(Timer1)

        'ensure MQTTT is connected and subscribed
        Timer2 = New System.Timers.Timer
        Timer2.Interval = 1000
        Timer2.Enabled = True
        AddHandler Timer2.Elapsed, AddressOf Timer2_Tick
        Timer2.Start()
        GC.KeepAlive(Timer2)

    End Sub
    Protected Overrides Sub OnStop()

    End Sub
    Private Sub Timer1_Tick(sender As Object, e As EventArgs)

        'if the webpage has not been updated after a specified time period update it to show that it is out of date

        Static WebpageIsShowingOutOfDateData As Boolean = False

        Timer1.Stop()  ' temporarily stop the timer as the proceessing below happens


        If Now <= DateAndTimeOfLastReading.AddMinutes(My.Settings.NewReadingsShouldBeAvailableInThisManyMinutes + 1) Then

            ' all is good (1 minute added above for tolerance)

            WebpageIsShowingOutOfDateData = False

        Else

            If WebpageIsShowingOutOfDateData Then

                ' webpage has already been updated to reflect its data is out of date

            Else

                Dim WarningPage As String

                If NewWebPage = String.Empty Then

                    ' First update is overdue
                    WarningPage = My.Resources.FirstUpdateOverdue.ToString
                    WindowsToLinuxFileTransfer(My.Settings.ApcheServer_IPAddress, My.Settings.ApcheServer_UserID, My.Settings.ApcheServer_Password, My.Settings.ApcheServer_Webpage_Filename, WarningPage)
                    WebpageIsShowingOutOfDateData = True

                Else

                    ' update the webpage to show that the data is out of date
                    WarningPage = NewWebPage.Replace("auto-style1", "auto-style2")
                    WindowsToLinuxFileTransfer(My.Settings.ApcheServer_IPAddress, My.Settings.ApcheServer_UserID, My.Settings.ApcheServer_Password, My.Settings.ApcheServer_Webpage_Filename, WarningPage)
                    WebpageIsShowingOutOfDateData = True

                End If

            End If

        End If

        Timer1.Start() ' resume the timer

    End Sub
    Private Sub Timer2_Tick(sender As Object, e As EventArgs)
        'ensure MQTTT is connected and subscribed

        Timer2.Stop() ' temporarily stop the timer as the proceessing below happens

        If (client IsNot Nothing) AndAlso (client.IsConnected) Then

            Timer2.Interval = 60 * 1000  ' set timer to tick again a minute to ensure connection remains good

        Else

            If ConnectAndSubscribe() Then

                Timer2.Interval = 60 * 1000  ' set timer to tick again a minute to ensure connection remains good

            Else

                ' the connection or subscription failed, try again later
                Timer2.Interval = My.Settings.OnFailedConnectionOrDisconncetRetryAfterThisManySeconds * 1000

            End If

        End If

        Timer2.Start() ' resume the timer

    End Sub
    Private Function ConnectAndSubscribe() As Boolean

        Dim ReturnValue As Boolean = False

        If Client_Connect_Request() Then

            ReturnValue = Client_Subscribe_Request()

        End If

        Return ReturnValue

    End Function
    Private Function Client_Connect_Request() As Boolean

        Dim ReturnValue As Boolean = False

        Try

            client = New MqttClient(My.Settings.MQTT_IP_Address.ToString)

            Dim clientId As String = Guid.NewGuid().ToString()

            AddHandler client.MqttMsgPublishReceived, AddressOf Client_MqttMsgPublishReceived
            AddHandler client.ConnectionClosed, AddressOf Client_Disconnect

            client.Connect(clientId)

            ReturnValue = client.IsConnected

        Catch ex As Exception

        End Try

        Return ReturnValue

    End Function
    Private Sub Client_Disconnect(sender As Object, e As EventArgs)

        Timer2.Interval = My.Settings.OnFailedConnectionOrDisconncetRetryAfterThisManySeconds * 1000
        Timer2.Start() ' Timer2 will attempt a reconnect  

    End Sub
    Private Function Client_Subscribe_Request() As Boolean

        Dim ReturnValue As Boolean = False

        If (client IsNot Nothing AndAlso client.IsConnected()) Then

            Try

                Dim Topics() As String = {"weather/temperature", "weather/pressure", "weather/humidity"}

                Dim Qos(Topics.Count - 1) As Byte
                Const DeiredQoS As Byte = 0

                For x = 0 To Topics.Count - 1
                    Qos(x) = DeiredQoS
                Next

                client.Subscribe(Topics, Qos)

                For x = 0 To Topics.Count - 1
                    Qos(x) = 0
                Next

                ReturnValue = True

            Catch ex As Exception
            End Try

            Return ReturnValue

        End If

    End Function

    Private Sub UpdateWebPage()

        NewWebPage = My.Resources.TemplateHTML.ToString
        NewWebPage = NewWebPage.Replace("%UPDATED%", DateAndTimeOfLastReading.ToString("yyyy-MM-dd h:mm:ss tt").ToLower)
        NewWebPage = NewWebPage.Replace("%TEMPERATURE%", Temperature)
        NewWebPage = NewWebPage.Replace("%HUMIDITY%", Humidity)
        NewWebPage = NewWebPage.Replace("%PRESSURE%", Pressure)
        WindowsToLinuxFileTransfer(My.Settings.ApcheServer_IPAddress, My.Settings.ApcheServer_UserID, My.Settings.ApcheServer_Password, My.Settings.ApcheServer_Webpage_Filename, NewWebPage)

    End Sub

    Private Sub LogResults()

        Dim LogEntry As String = DateAndTimeOfLastReading.ToString("yyyy-MM-dd HH:mm:ss.fff") & ", " & Temperature & ", " & Humidity & ", " & Pressure & vbCrLf
        IO.File.AppendAllText(gProgramData_filename, LogEntry)

    End Sub

    Private Sub Client_MqttMsgPublishReceived(ByVal sender As Object, ByVal e As MqttMsgPublishEventArgs)

        ' Temperature is received first
        ' Humidity is received second
        ' Pressure is received third

        Try

            If e.Topic = "weather/temperature" Then

                DateAndTimeOfLastReading = Now
                Temperature = Encoding.Default.GetString(e.Message).Trim

                'restart the timer which would otherwise reports late data in another minute
                Timer2.Stop()
                Timer2.Start()

            ElseIf e.Topic = "weather/humidity" Then

                Humidity = Encoding.Default.GetString(e.Message)

            ElseIf e.Topic = "weather/pressure" Then

                Pressure = Encoding.Default.GetString(e.Message).Trim

                UpdateWebPage()
                LogResults()

            End If

        Catch ex As Exception

        End Try

    End Sub
    Friend Shared Sub WindowsToLinuxFileTransfer(ByVal Linux_Host_IP_Address As String, ByVal Linux_UserID As String, ByVal Linux_Password As String, ByVal LinuxPathAndFilename As String, ByVal Webpage As String)

        ' used to upload the contents of a string to a Linux file 
        ' ref: https://gist.github.com/piccaso/d963331dcbf20611b094

        ' the following linux commands were needed to allow the userid "pi" to write to the Apache www directory:
        '
        ' sudo gpasswd -a "pi" www-data
        ' sudo chown -R "pi":www-data /var/www
        ' find /var/www -type f -exec chmod 0660 {} \;
        ' sudo find /var/www -type d -exec chmod 2770 {} \;
        '
        ' ref: https://askubuntu.com/questions/46331/how-to-avoid-using-sudo-when-working-in-var-www

        Try

            Const SFTP_Port As Integer = 22

            Dim LinuxPathName As String = LinuxPathAndFilename.Remove(LinuxPathAndFilename.LastIndexOf("/"))
            Dim LinuxFileName As String = LinuxPathAndFilename.Remove(0, LinuxPathAndFilename.LastIndexOf("/") + 1)

            ' convert string to stream
            Dim byteArray() As Byte = Encoding.ASCII.GetBytes(Webpage)
            Dim MemoryStream As IO.MemoryStream = New IO.MemoryStream(byteArray)
            MemoryStream.Seek(0, IO.SeekOrigin.Begin)

            Dim ConnNfo As ConnectionInfo = New ConnectionInfo(Linux_Host_IP_Address, SFTP_Port, Linux_UserID, New AuthenticationMethod() {New PasswordAuthenticationMethod(Linux_UserID, Linux_Password)})
            Dim sftp = New SftpClient(ConnNfo)

            With sftp
                .Connect()
                .ChangeDirectory(LinuxPathName)
                .UploadFile(MemoryStream, LinuxFileName, True)
                .Disconnect()
                .Dispose()
            End With

            ConnNfo = Nothing

        Catch ex As Exception

        End Try

    End Sub

End Class
