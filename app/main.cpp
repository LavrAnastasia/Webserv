int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // Try
    //  Parse, Validate, Normalize Config (config module)

    //  Run HTTP Server (app folder class)
    //      Run TCP Server (net module)
    //          Create, Bind, Listen, Register sockets
    //      Run Event Loop (net module)
    //          Runs the infinite event cycle
    //          Waits events from Poller
    //          Create Connection Entity
    //          Stores Connection Entity in ConnectionRegistry
    //          Event readable ? ->
    //              bad/ closed/ not enough data ? skip or do some work
    //              Connection -> get HttpRequest
    //              Request Handler ->  make HttpResponse from HttpRequest
    //              Serialize Response to bytes
    //              make event ready to write
    //          Event Writable?
    //          write
    // Catch
}
