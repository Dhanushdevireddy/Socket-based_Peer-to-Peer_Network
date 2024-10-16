#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <bits/stdc++.h>
#include <thread>
#include <arpa/inet.h>
#include <bits/stdc++.h>

using namespace std;

map<string,vector<string>> users_list;
map<int, string> current_logged_in_clients;
map<string, string> current_logged_in_clients1;
map<string,vector<string>> groups_list;
map<string,vector<string>> join_requests;
map<vector<string>,vector<vector<string>>> files_list; //files_list[{grp_name,file_name}] = {{current_logged_in_clients[client_socket]},filepath in their respective system,hashes};

void send_message_to_client(int client_socket,string message){
    ssize_t bytes_sent = send(client_socket, &message[0], message.size(), 0);
    if (bytes_sent == -1) {
        cout<<"Error sending message to client.\n";
        close(client_socket);
    }
}

void create_user(int client_socket,vector<string> tokens){

    if(tokens.size()<3){
        send_message_to_client(client_socket,"Need atleast 2 arguments.\n");
        return;
    }

    map<string, vector<string>>::iterator it = users_list.begin();
    while(it != users_list.end()){
        if(it->first==tokens[1]){
            send_message_to_client(client_socket,"User Id already in use.\n");
            return;
        }
        it++;
    }
    users_list[tokens[1]].push_back(tokens[2]);

    send_message_to_client(client_socket,"User creation success.\n");
}

void login(int client_socket,vector<string> tokens){
    
    if(tokens.size()<4){
        send_message_to_client(client_socket,"Need atleast 2 arguments.\n");
        return;
    }

    map<string, vector<string>>::iterator it = users_list.begin();
    while(it != users_list.end()){
        if(it->first==tokens[1]){
            if(it->second[0]==tokens[2]){
                current_logged_in_clients[client_socket] = it->first;
                current_logged_in_clients1[it->first] = tokens[3];
                send_message_to_client(client_socket, "Login Successful.\n");
                return;
            }else{
                send_message_to_client(client_socket,"Wrong Password");
                return;
            }
        }
        it++;
    }
    send_message_to_client(client_socket,"User Id doesnt exist.\n");
}

void create_group(int client_socket,vector<string> tokens){

    if(tokens.size()<2){
        send_message_to_client(client_socket,"Need atleast 1 arguments.\n");
        return;
    }

    map<string, vector<string>>::iterator it = groups_list.begin();
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            send_message_to_client(client_socket,"Group Id already in use.\n");
            return;
        }
        it++;
    }
    groups_list[tokens[1]] = {current_logged_in_clients[client_socket]};
    
    send_message_to_client(client_socket,"Group creation success.\n");
}

void join_group(int client_socket,vector<string> tokens){

    map<string, vector<string>>::iterator it = groups_list.begin();
    bool group_present = false;
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            group_present = true;
            break;
        }
        it++;
    }
    if(!group_present){
        send_message_to_client(client_socket,"Group doesn't exist.\n");
        return;
    }

    if(tokens.size()<2){
        send_message_to_client(client_socket,"Need atleast 1 arguments.\n");
        return;
    }

    bool added_flag = false;
    it = join_requests.begin();
    while(it != join_requests.end()){
        if(it->first==tokens[1]){
            it->second.push_back(current_logged_in_clients[client_socket]);
            added_flag = true;
            break;
        }
        it++;
    }
    if(!added_flag){
        join_requests[tokens[1]] = {current_logged_in_clients[client_socket]};
    }

    send_message_to_client(client_socket,"Join Request Sent Successfully.\n");
    return;
    
}

void leave_group(int client_socket,vector<string> tokens){

    if(tokens.size()<2){
        send_message_to_client(client_socket,"Need atleast 1 arguments.\n");
        return;
    }

    map<string, vector<string>>::iterator it = groups_list.begin();
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            auto it1 = find(it->second.begin(), it->second.end(), current_logged_in_clients[client_socket]);
            if (it1 != it->second.end()) {
                it->second.erase(it1);
                send_message_to_client(client_socket,"Left Group Successfully.\n");
            }else{
                send_message_to_client(client_socket,"You are not in this group.\n");
            }
            return;
        }
        it++;
    }
    send_message_to_client(client_socket,"No such Group Exists.\n");
}

void list_requests(int client_socket,vector<string> tokens){

    if(tokens.size()<2){
        send_message_to_client(client_socket,"Need atleast 1 arguments.\n");
        return;
    }

    map<string, vector<string>>::iterator it = groups_list.begin();
    bool group_present = false;
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            if(it->second[0]!=current_logged_in_clients[client_socket]){
                send_message_to_client(client_socket,"Unauthorized!!!\n");
                return;
            }
            group_present = true;
            break;
        }
        it++;
    }
    if(!group_present){
        send_message_to_client(client_socket,"No such Group exists.\n");
        return;
    }

    it = join_requests.begin();
    string list_of_requests = "";
    while(it != join_requests.end()){
        if(it->first==tokens[1]){
            for(unsigned long int i =0;i<it->second.size();i++){
                list_of_requests = list_of_requests + it->second[i] + "\n";
            }
            if(list_of_requests==""){
                send_message_to_client(client_socket,"No Requests are there at this moment.\n");
                return;
            }
            send_message_to_client(client_socket,list_of_requests);
            return;
        }
        it++;
    }
}

void accept_request(int client_socket,vector<string> tokens){

    if(tokens.size()<3){
        send_message_to_client(client_socket,"Need atleast 2 arguments.\n");
        return;
    }

    //checking if the group exists and the given client is the owner
    map<string, vector<string>>::iterator it = groups_list.begin();
    bool group_present = false;
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            if(it->second[0]!=current_logged_in_clients[client_socket]){
                send_message_to_client(client_socket,"Unauthorized!!!\n");
                return;
            }
            group_present = true;
            break;
        }
        it++;
    }
    if(!group_present){
        send_message_to_client(client_socket,"No such Group exists.\n");
        return;
    }

    //checking if the user exists
    it = users_list.begin();
    bool user_exists = false;
    while(it != users_list.end()){
        if(it->first==tokens[2]){
            user_exists = true;
            break;
        }
        it++;
    }
    if(!user_exists){
        send_message_to_client(client_socket,"User doesn't exist.\n");
        return;
    }

    //checking if the client sent the join request
    it = join_requests.begin();
    while(it != join_requests.end()){
        if(it->first==tokens[1]){
            auto it1 = find(it->second.begin(), it->second.end(), tokens[2]);
            if (it1 != it->second.end()) {
                it->second.erase(it1);
            }else{
                send_message_to_client(client_socket,"User didn't send join request.\n");
                return;
            }
        }
        it++;
    }

    //check if he's already part of the group
    it = groups_list.begin();
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            auto it1 = find(it->second.begin(), it->second.end(), tokens[2]);
            if (it1 != it->second.end()) {
                send_message_to_client(client_socket,"Client already is part of the group.\n");
                return;
            }
            break;
        }
        it++;
    }

    //adding the client to the group
    it = groups_list.begin();
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            it->second.push_back(tokens[2]); 
            send_message_to_client(client_socket,"Request Accepted Successfully.\n");
            return;
        }
        it++;
    }

}

void list_groups(int client_socket){

    map<string, vector<string>>::iterator it = groups_list.begin();
    string list_of_groups = "";
    while(it != groups_list.end()){
        list_of_groups = list_of_groups + it->first + "\n";
        it++;
    }
    if(list_of_groups==""){
        send_message_to_client(client_socket,"No groups are there at this moment.\n");
        return;
    }
    send_message_to_client(client_socket,list_of_groups);
}

void upload_file(int client_socket, vector<string> tokens){

    if(tokens.size()<4){
        send_message_to_client(client_socket,"Need atleast 2 arguments.\n");
        return;
    }

    string file_path = tokens[1];
    size_t pos = file_path.find_last_of("/\\");
    string file_name;
    if (pos != std::string::npos) {
        file_name = file_path.substr(pos + 1);
    }else{
        file_name = file_path;
    }
    
    string grp_name = tokens[2];

    vector<string> hashes (tokens.begin()+3,tokens.end());
    
    if (files_list.find({grp_name,file_name}) != files_list.end()) {
        files_list[{grp_name,file_name}][0].push_back(current_logged_in_clients[client_socket]);
        files_list[{grp_name,file_name}][1].push_back(file_path);
    } else {
        files_list[{grp_name,file_name}] = {{current_logged_in_clients[client_socket]},{file_path},hashes};
    }
    send_message_to_client(client_socket,"File uploaded successfully.\n");
    
}

void list_files(int client_socket, vector<string> tokens){

    if(tokens.size()<2){
        send_message_to_client(client_socket,"Need atleast 2 arguments.\n");
        return;
    }
    map<string, vector<string>>::iterator it = groups_list.begin();
    bool group_present = false;
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            group_present = true;
            auto it1 = find(it->second.begin(), it->second.end(), current_logged_in_clients[client_socket]);
            if (it1 == it->second.end()) {
                send_message_to_client(client_socket,"You are not in this group.\n");
                return;
            }
            break;
        }
        it++;
    }
    if(!group_present){
        send_message_to_client(client_socket,"Group doesn't exist.\n");
        return;
    }
    
    map<vector<string>,vector<vector<string>>>:: iterator it0 = files_list.begin();
    string message = "";
    while(it0!=files_list.end()){
        if(it0->first[0]==tokens[1]){
            message = message + it0->first[1] + "\n";
        }
        it0++;
    }
    if(message==""){
        send_message_to_client(client_socket,"No Files in the grp as of now.\n");
        return;
    }
    send_message_to_client(client_socket,message);
}

void download_file(int client_socket, vector<string> tokens){
    if(tokens.size()<4){
        send_message_to_client(client_socket,"Need atleast 4 arguments.\n");
        return;
    }
    map<string, vector<string>>::iterator it = groups_list.begin();
    bool group_present = false;
    while(it != groups_list.end()){
        if(it->first==tokens[1]){
            group_present = true;
            auto it1 = find(it->second.begin(), it->second.end(), current_logged_in_clients[client_socket]);
            if (it1 == it->second.end()) {
                send_message_to_client(client_socket,"You are not in this group.\n");
                return;
            }
            break;
        }
        it++;
    }
    if(!group_present){
        send_message_to_client(client_socket,"Group doesn't exist.\n");
        return;
    }

    auto it1 = files_list.find({tokens[1],tokens[2]});
    if(it1 == files_list.end()){
        send_message_to_client(client_socket, "No such file exists.\n");
        return;
    }

    string message = "Download;"+tokens[2]+";"+tokens[3]+";"+"Client_info;";
    
    for(int i =0;i<(int)it1->second[0].size();i++){
        if(current_logged_in_clients1.find(it1->second[0][i]) != current_logged_in_clients1.end()) {
        message += current_logged_in_clients1[it1->second[0][i]] + ";";
        }
    }

    message = message + "Paths;";
    for(int i =0;i<(int)it1->second[0].size();i++){
        if(current_logged_in_clients1.find(it1->second[0][i]) != current_logged_in_clients1.end()) {
        message += current_logged_in_clients1[it1->second[1][i]] + ";";
        }
    }

    message = message + "Hashes;";
    for(int i =0;i<(int)it1->second[2].size();i++){
        message = message + it1->second[2][i] + ";";
    }

    send_message_to_client(client_socket, message);

}

void logout(int client_socket){
    string username = current_logged_in_clients[client_socket];
    current_logged_in_clients.erase(client_socket);
    current_logged_in_clients1.erase(username);
}

void stop_sharing(int client_socket, vector<string> tokens){
    string user_name = current_logged_in_clients[client_socket];
    int index;
    for(int i =0;i<(int)files_list[{tokens[1],tokens[2]}][0].size();i++){
        if(files_list[{tokens[1],tokens[2]}][0][i] == user_name){
            index = i;
            break;
        }
    }
    files_list[{tokens[1],tokens[2]}][0].erase(files_list[{tokens[1],tokens[2]}][0].begin()+index);
    files_list[{tokens[1],tokens[2]}][1].erase(files_list[{tokens[1],tokens[2]}][1].begin()+index);
    send_message_to_client(client_socket,"Stopped sharing Successfully.\n");
}

void handle_client_query(int client_socket,vector<string> tokens){

    if(tokens[0]=="create_user"){

        create_user(client_socket,tokens);

    }else if(tokens[0]=="login"){

        login(client_socket,tokens);

    }else if(tokens[0]=="create_group"){

        create_group(client_socket,tokens);

    }else if(tokens[0]=="join_group"){

        join_group(client_socket,tokens);

    }else if(tokens[0]=="leave_group"){

        leave_group(client_socket,tokens);

    }else if(tokens[0]=="list_requests"){

        list_requests(client_socket,tokens);

    }else if(tokens[0]=="accept_request"){

        accept_request(client_socket,tokens);

    }else if(tokens[0]=="list_groups"){

        list_groups(client_socket);

    }else if(tokens[0] == "list_files"){

        list_files(client_socket, tokens);
    
    }else if(tokens[0]=="upload_file"){

        upload_file(client_socket,tokens);

    }else if(tokens[0]=="download_file"){

        download_file(client_socket,tokens);

    }else if(tokens[0]=="logout"){
        
        logout(client_socket);

    }else if(tokens[0]=="stop sharing"){

        stop_sharing(client_socket, tokens);

    }else{
        send_message_to_client(client_socket,"Give Proper Input.\n");
    }
}

vector<string> tokenizer(string command){
    vector<string> tokens;

    const char* delimiters = " \t";

    char temp[command.size()];
    strcpy(temp,command.c_str());

    char* token = strtok(temp, delimiters);

    while (token != nullptr) {
        tokens.push_back(token);
        token = strtok(nullptr, delimiters);
    }
    
    return tokens;
}

void handle_client(int client_socket){
    while(true){
        string client_query;
        client_query.resize(525000);
        ssize_t bytes_recieved = recv(client_socket, &client_query[0], 525000, 0);
        if(bytes_recieved<0){
            cout<<"Recv failed.\n";
            return;
        }else if(bytes_recieved==0){
            return;
        }
        if (bytes_recieved > 0) {
            client_query.resize(bytes_recieved);
        }
        vector<string> tokens = tokenizer(client_query);
        handle_client_query(client_socket,tokens);
        if(tokens[0]=="logout"){
            break;
        }
    }
    
    close(client_socket);
}

void run_tracker(int port, string address){
    int tracker_socket = socket(AF_INET, SOCK_STREAM,0);
    if(tracker_socket == -1){
        cout<<"Failed to create socket on port: "<<port<<"\n";
        exit(0);
    }

    sockaddr_in tracker_address;
    tracker_address.sin_family = AF_INET;
    tracker_address.sin_port = htons(port);
    tracker_address.sin_addr.s_addr = inet_addr(&address[0]);

    if(bind(tracker_socket, (struct sockaddr*)&tracker_address, sizeof(tracker_address)) == -1){
        cout<<"Failed to bind on port: "<<port<<"\n";
        close(tracker_socket);
        exit(0);
    }

    if(listen(tracker_socket, 50) == -1){
        cout<<"Failed to listen on port: "<<port<<"\n";
        close(tracker_socket);
        exit(0);
    }

    cout<<"Tracker running on port: "<<port<<"\n";

    vector <thread> client_threads;

    while(true){
        int client_socket = accept(tracker_socket, nullptr, nullptr);
        if(client_socket<0){
            cout<<"Accept failed.\n";
            continue;
        }
        client_threads.push_back(thread(handle_client,client_socket));
        client_threads.back().detach();
    }

    close(tracker_socket);
}

void get_input(){
    string input;
    while(true){
        cin>>input;
        if(input=="quit"){
            exit(0);
        }else{
            cout<<"Give proper input!!\n";
        }
    }
}


int main(int argc, char* argv[]){

    if(argc<3){
        cout<<"2 arguments are required!!\n";
        exit(0);
    }

    string tracker_info_filename = argv[1];

    int fd = open(&tracker_info_filename[0], O_RDONLY);
    if (fd == -1) {
        cout<<"Error opening file\n";
        exit(0);
    }

    int tracker_number;
    try{
        tracker_number = stoi(argv[2]);
    }catch(...){
        cout<<"Tracker number should be either 0 or 1.\n";
        exit(0);
    }
    if(tracker_number!=0 && tracker_number!=1){
        cout<<"Tracker number should be either 0 or 1.\n";
        exit(0);
    }
    
    string tracker_address_and_port="";

    char buffer[1024];
    int current_line_index =0;
    ssize_t bytes_read;
    bool flag = true;

    while((bytes_read = read(fd,buffer, 1024))>0){
        if(flag){
            for(int i =0;i<bytes_read;i++){
                char c = buffer[i];
                if(c=='\n'){
                    if(current_line_index==tracker_number){
                        flag = false;
                        break;
                    }
                    tracker_address_and_port = "";
                    current_line_index++;
                }else{
                    tracker_address_and_port += c;
                }
            }
        }
    }

    close(fd);

    string tracker_address;
    int port;
    unsigned long int partition;

    for(unsigned long int i =0;i<tracker_address_and_port.size();i++){
        if(tracker_address_and_port[i]==':'){
            partition = i;
            break;
        }
    }

    tracker_address = tracker_address_and_port.substr(0,partition);
    port = stoi(tracker_address_and_port.substr(partition+1));

    thread handling_tracker_functions(run_tracker,port, tracker_address);
    thread handling_input(get_input);

    handling_tracker_functions.join();
    handling_input.join();

    return 0;
}