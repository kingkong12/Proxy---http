# Proxy http


It is a basic proxy server(ONLY HTTP GET) developed in C without using any external libraries apart from the system available ones. Also the proxy server is a one request at a time service
To run the code(Mac/ Linux):


Download the file proxy.c

Use the following commands: 
//To Compile the proxy 
***
gcc -o proxy proxy.c
***
//To Run the proxy: 
***
./proxy PORT_NUMBER &

With this you would be able to see the logs in the terminal.
To request to the proxy, Change the HTTP Proxy server browser settings to local host IP and the Port Number specified earlier.
If any doubts or improvements, mail me at: vasanimithil999@gmail.com
