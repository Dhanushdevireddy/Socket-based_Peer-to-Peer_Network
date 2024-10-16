#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <openssl/sha.h>
#include <random>
#include <sys/stat.h>

using namespace std;

bool logged_in = false;

int global_client_socket;

map<string, string> file_name_to_path;
map<string,map<int, vector<string>>> leechers_list;
map<string, vector<bool>> leeching_tracker;

string hash_chunk(unsigned char *chunk, size_t size){
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(chunk, size, hash);

    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        hash[i] = static_cast<unsigned char>(i);
    }

    string hex_str;
    hex_str.reserve(SHA_DIGEST_LENGTH * 2);

    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        hex_str += (hash[i] / 16 < 10 ? '0' + (hash[i] / 16) : 'a' + (hash[i] / 16 - 10));
        hex_str += (hash[i] % 16 < 10 ? '0' + (hash[i] % 16) : 'a' + (hash[i] % 16 - 10));
    }
    return hex_str;
}

vector<string> SHA1_hash_function(vector<string> tokens){
    vector<string> hashes;

    int fd = open(&tokens[1][0], O_RDONLY);
    if (fd==-1) {
        cout<<"Error opening file: "<<tokens[1]<<"\n";
        exit(0);
    }

    int chunk_size = 512*1024;
    unsigned char *buffer = (unsigned char *)malloc(chunk_size);
    if (!buffer) {
        cout<<"Memory allocation failed.\n";
        close(fd);
        exit(0);
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, chunk_size)) > 0) {
        string hash_str = hash_chunk(buffer, bytes_read);
        hashes.push_back(hash_str);
    }
    
    if (bytes_read < 0) {
        cout<<"Error reading file: "<<tokens[1]<<"\n";
    }

    free(buffer);
    close(fd);
    return hashes;
}

vector<string> tokenizer(string command, string delimiter){
    vector<string> tokens;

    const char* delimiters = &delimiter[0];

    char temp[command.size()];
    strcpy(temp,command.c_str());

    char* token = strtok(temp, delimiters);

    while (token != nullptr) {
        tokens.push_back(token);
        token = strtok(nullptr, delimiters);
    }
    
    return tokens;
}

int generate_random_int(int min, int max) {
    random_device rd;
    mt19937 gen(rd());
    
    uniform_int_distribution<> dist(min, max);

    return dist(gen);
}

void send_piece(int client_socket, int leecher_socket, string filepath, int piece_number) {
    const size_t PIECE_SIZE = 512 * 1024; 
    const size_t SUBPIECE_SIZE = 32 * 1024; 
    char buffer[SUBPIECE_SIZE];

    int file_fd = open(filepath.c_str(), O_RDONLY);
    if (file_fd == -1) {
        cout << "Error opening file: " << filepath << "\n";
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) == -1) {
        cout << "Error getting file size\n";
        close(file_fd);
        return;
    }
    off_t file_size = file_stat.st_size;

    off_t offset = 0;
    if (piece_number != -1) {
        offset = PIECE_SIZE * piece_number;
        if (lseek(file_fd, offset, SEEK_SET) == -1) {
            cout << "Error seeking file\n";
            close(file_fd);
            return;
        }
    }

    size_t bytes_to_send = min(PIECE_SIZE, static_cast<size_t>(file_size - offset));

    while (bytes_to_send > 0) {
        ssize_t bytes_read = read(file_fd, buffer, min(SUBPIECE_SIZE, bytes_to_send));
        if (bytes_read <= 0) break;

        ssize_t bytes_sent = send(leecher_socket, buffer, bytes_read, 0);
        if (bytes_sent <= 0) break;

        bytes_to_send -= bytes_read;
    }

    close(file_fd);
    //close(leecher_socket);
}

void handle_leecher_request(int client_socket, int leecher_socket){
    client_socket = global_client_socket;
    while(true){
        string client_query;
        client_query.resize(525000);
        ssize_t bytes_recieved = recv(leecher_socket, &client_query[0], 525000, 0);
        if(bytes_recieved<0){
            cout<<"Recv failed.\n";
            return;
        }else if(bytes_recieved==0){
            return;
        }
        if (bytes_recieved > 0) {
            client_query.resize(bytes_recieved);
        }
        vector<string> tokens = tokenizer(client_query,";");
        
        if(tokens[0] == "PRequest"){
            string file_name = tokens[1];
            string file_path = file_name_to_path[file_name];
            int piece_number = stoi(tokens[2]);
            bool all_true = true;
            for(int i =0;i<(int)leeching_tracker.size();i++){
                if(leeching_tracker[file_name][piece_number]==false){
                    all_true = false;
                    break;
                }
            }
            if(all_true){
                string message = "Leechers;";
                for(auto it = leechers_list[file_name].begin();it!=leechers_list[file_name].end();it++){
                    message = message + to_string(it->first) + ",";
                    for(int i =0;i<(int)it->second.size();i++){
                        message = message + it->second[i] + ",";
                    }
                    message = message + ";";
                }
                ssize_t bytes_sent = send(leecher_socket, &message[0], message.size(),0);
                close(leecher_socket);
                break;
            }
            send_piece(client_socket, leecher_socket, file_path, piece_number);
            if(leechers_list[file_name].find(piece_number)!=leechers_list[file_name].end()){
                leechers_list[file_name][piece_number].push_back(tokens[3]);
            }else{
                leechers_list[file_name][piece_number] = {tokens[3]};
            }
            leeching_tracker[file_name][piece_number] = true;
        }else if(tokens[0]=="LRequest"){
            string file_name = tokens[1];
            string file_path = file_name_to_path[file_name];
            int piece_number = stoi(tokens[2]);
            file_path = file_path + "_piece_" + to_string(piece_number);
            send_piece(client_socket, leecher_socket,file_path, -1);
        }
    }
    
    close(leecher_socket);
}

void receive_requests(string address, int port){
    int client_socket1 = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(port);
    client_address.sin_addr.s_addr = inet_addr(&address[0]);

    if(bind(client_socket1, (struct sockaddr*)&client_address, sizeof(client_address)) == -1){
        cout<<"Failed to bind on port: "<<port<<"\n";
        close(client_socket1);
        exit(0);
    }

    if(listen(client_socket1, 50) == -1){
        cout<<"Failed to listen on port: "<<port<<"\n";
        close(client_socket1);
        exit(0);
    }
    vector<thread> leecher_threads;
    global_client_socket = client_socket1;
    while(true){
        int leecher_socket = accept(client_socket1, nullptr, nullptr);
        if(leecher_socket<0){
            cout<<"Accept failed.\n";
            continue;
        }

        leecher_threads.push_back(thread(handle_leecher_request,client_socket1,leecher_socket));
        leecher_threads.back().detach();
    }
}

void handle_leecher( map<int,vector<string>> leechers, int i, string file_name, string destination_path, vector<string> hashes, bool& corrupted){
    size_t colon_pos1 = leechers[i][0].find(":");
    string leecher_address =  leechers[i][0].substr(0, colon_pos1);
    int leecher_port = stoi(leechers[i][0].substr(colon_pos1 + 1));

    int leecher_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (leecher_socket == -1) {
        cout << "Failed to create socket for leecher.\n";
        return;
    }

    sockaddr_in leecher_addr;
    leecher_addr.sin_family = AF_INET;
    leecher_addr.sin_port = htons(leecher_port);
    leecher_addr.sin_addr.s_addr = inet_addr(leecher_address.c_str());

    if (connect(leecher_socket, (struct sockaddr*)&leecher_addr, sizeof(leecher_addr)) == -1) {
        cout<<"Failed to connect to leecher at "<< leecher_address << ":" << leecher_port << "\n";
        close(leecher_socket);
        return;
    }

    string message = "LPRequest;" + file_name  + ";" + to_string(i);

    ssize_t bytes_sent = send(leecher_socket, message.c_str(), message.size(), 0);
    if (bytes_sent < 0) {
        cout << "Failed to send piece request to seeder.\n";
        close(leecher_socket);
        return;
    }

    const size_t chunk_size = 32768; 
    const size_t total_size = 512 * 1024;
    vector<unsigned char> received_data(total_size);
    size_t total_bytes_received = 0;

    while (total_bytes_received < total_size) {
        vector<unsigned char> buffer(32768);
        ssize_t bytes_received = recv(leecher_socket, reinterpret_cast<char*>(&buffer[0]), buffer.size(), 0);

        if (bytes_received < 0) {
            close(leecher_socket);
            break;
        } else if (bytes_received == 0) {
            close(leecher_socket);
            break;
        }

        string buffer_str(buffer.begin(), buffer.begin() + bytes_received);
        if (buffer_str.substr(0,8)=="Leechers") {
            vector<string> tokens = tokenizer(buffer_str, ";");
            for (int i1 = 1; i1 < (int)tokens.size(); i1++) {
                vector<string> parts = tokenizer(tokens[i1], ",");
                leechers[stoi(parts[0])] = vector<string>(parts.begin() + 1, parts.end());
            }
        }

        received_data.insert(received_data.begin() + total_bytes_received, buffer.begin(), buffer.begin() + bytes_received);
        total_bytes_received += bytes_received;
    }



    string new_file_name = destination_path + "/" + file_name + "_piece_" + to_string(i);

    int fd = open(new_file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        cout << "fd error\n";
        close(leecher_socket);
        return;
    }

    ssize_t bytes_written = write(fd, reinterpret_cast<char*>(&received_data[0]), total_bytes_received);
    if (bytes_written == -1) {
        cout << "Error writing to file.\n";
    }
    close(fd);

    string new_hash = SHA1_hash_function({"",new_file_name})[0];

    if(new_hash!=hashes[i]){
        corrupted = true;
    }

    close(leecher_socket);
}

void concatenate_pieces(string destination_path, string file_name, int total_pieces) {
    string output_file_name = destination_path + "/" + file_name;
    int output_fd = open(output_file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    
    if (output_fd == -1) {
        cout << "Error opening output file: " << output_file_name << "\n";
        return;
    }

    for (int piece_number = 0; piece_number < total_pieces; ++piece_number) {
        string piece_file_name = destination_path + "/" + file_name + "_piece_" + to_string(piece_number);
        int input_fd = open(piece_file_name.c_str(), O_RDONLY);
        
        if (input_fd == -1) {
            cout << "Error opening piece file: " << piece_file_name << "\n";
            continue;
        }

        char buffer[32768];
        ssize_t bytes_read;
        
        while ((bytes_read = read(input_fd, buffer, sizeof(buffer))) > 0) {
            ssize_t bytes_written = write(output_fd, buffer, bytes_read);
            if (bytes_written == -1) {
                cout << "Error writing to output file: " << output_file_name << "\n";
                break;
            }
        }

        if (bytes_read == -1) {
            cout << "Error reading from piece file: " << piece_file_name << "\n";
        }

        close(input_fd);
        if (remove(piece_file_name.c_str()) != 0) {
            cout << "Error deleting piece file: " << piece_file_name << "\n";
        }
    }

    close(output_fd);
    cout << "Files concatenated successfully into: " << output_file_name << "\n";
}

void handle_download(string s, int client_socket, string address, int port){
    
    vector<string> tokens = tokenizer(s,";");
    //client_socket = global_client_socket;
    string file_name = tokens[1];
    string destination_path = tokens[2];
    file_name_to_path[file_name] = destination_path;

    vector<string> seeder_sockets;
    vector<string> file_path_in_seeder;
    int i =4;
    for(;i<(int)tokens.size();i++){
        if(tokens[i]!="Paths"){
            seeder_sockets.push_back(tokens[i]);
        }else{
            break;
        }
    }
    i++;
    for(;i<(int)tokens.size();i++){
        if(tokens[i]!="Hashes"){
            file_path_in_seeder.push_back(tokens[i]);
        }else{
            break;
        }
    }
    vector<string> hashes;
    i++;
    for(;i<(int)tokens.size();i++){
        hashes.push_back(tokens[i]);
    }

    size_t colon_pos = seeder_sockets[0].find(":");
    string seeder_address =  seeder_sockets[0].substr(0, colon_pos);
    int seeder_port = stoi(seeder_sockets[0].substr(colon_pos + 1));

    int seeder_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (seeder_socket == -1) {
        cout << "Failed to create socket for seeder.\n";
        return;
    }

    sockaddr_in seeder_addr;
    seeder_addr.sin_family = AF_INET;
    seeder_addr.sin_port = htons(seeder_port);
    seeder_addr.sin_addr.s_addr = inet_addr(seeder_address.c_str());

    if (connect(seeder_socket, (struct sockaddr*)&seeder_addr, sizeof(seeder_addr)) == -1) {
        cout<<"Failed to connect to seeder at "<< seeder_address << ":" << seeder_port << "\n";
        close(seeder_socket);
        return;
    }

    int received_pieces = 0;
    vector<bool> received ((int)hashes.size(),false);

    map<int,vector<string>> leechers;
    bool break_flag = false;
    bool corrupted = false;
    while(received_pieces<(int)hashes.size()){
        int piece_number;
        if(hashes.size()>1){
            piece_number = generate_random_int(0,(int)hashes.size()-1);
        }else{
            piece_number = 0;
        }
        if(received[piece_number]==true){
            continue;
        }

        string message = "PRequest;" + file_name  + ";" + to_string(piece_number) + ";" + address + ":" + to_string(port);
        ssize_t bytes_sent = send(seeder_socket, message.c_str(), message.size(), 0);
        if (bytes_sent <= 0) {
            cout << "Failed to send piece request to seeder.\n";
            close(seeder_socket);
            break;
        }

        const size_t chunk_size = 32768; 
        const size_t total_size = 512 * 1024;
        vector<unsigned char> received_data(total_size);  
        size_t total_bytes_received = 0;

        while (total_bytes_received < total_size) {
            vector<unsigned char> buffer(32768);  
            ssize_t bytes_received = recv(seeder_socket, reinterpret_cast<char*>(&buffer[0]), buffer.size(), 0);

            if (bytes_received < 0) {
                close(seeder_socket);
                break;
            } else if (bytes_received == 0) {
                close(seeder_socket);
                break;
            }

            string buffer_str(buffer.begin(), buffer.begin() + bytes_received);
            if (buffer_str.substr(0,8) == "Leechers") {
                vector<string> tokens = tokenizer(buffer_str, ";");
                for (int i = 1; i < (int)tokens.size(); i++) {
                    vector<string> parts = tokenizer(tokens[i], ",");
                    leechers[stoi(parts[0])] = vector<string>(parts.begin() + 1, parts.end());
                }
                break_flag = true;
                break;
            }

            received_data.insert(received_data.begin() + total_bytes_received, buffer.begin(), buffer.begin() + bytes_received);
            total_bytes_received += bytes_received;
        }
        if(break_flag){
            break;
        }

        string new_file_name = destination_path + "/" + file_name + "_piece_" + to_string(piece_number);

        int fd = open(new_file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd == -1) {
            cout << "Error creating file.\n";
            close(seeder_socket);
            continue;
        }

        ssize_t bytes_written = write(fd, reinterpret_cast<char*>(&received_data[0]), total_bytes_received);
        if (bytes_written == -1) {
            cout << "Error writing to file.\n";
        }
        close(fd);

        received[piece_number] = true;
        received_pieces++;

        string new_hash = SHA1_hash_function({"",new_file_name})[0];
        if(new_hash!=hashes[piece_number]){
            corrupted = true;
            break;
        }
        
    }
    close(seeder_socket);
    if(corrupted){
        cout<<"File is corrupted.\n";
        return;
    }

    vector<thread> leecher_threads;

    for(int i =0;i<(int)received.size();i++){
        if(!received[i]){
            leecher_threads.push_back(thread(handle_leecher, leechers, i, file_name, destination_path, hashes, ref(corrupted)));
            leecher_threads.back().join();
        }
    }

    if(corrupted){
        cout<<"File is corrupted.\n";
        return;
    }

    concatenate_pieces(destination_path,file_name, hashes.size());
}

void handle_response_from_tracker(int client_socket, string address, int port){
    while (true) {
        string response;
        response.resize(525000);
        ssize_t bytes_received = recv(client_socket, &response[0], response.size(), 0);
        if (bytes_received < 0) {
            cout<<"Error receiving response from tracker.\n";
            continue;
        } else if (bytes_received == 0) {
            cout<<"Connection disconnected.\n";
            exit(0);
        }
        if(bytes_received>0){
            response.resize(bytes_received);
        }
        if(response=="Login Successful.\n"){
            logged_in = true;
        }
        vector<thread> download_threads;
        if(response.substr(0,8)=="Download"){
            cout<<"Downloading...\n";
            download_threads.push_back(thread(handle_download, response, client_socket, address, port));
            download_threads.back().detach();
        }else{
            cout<<response;
        }
    }
}

void connect_tracker(int client_socket, vector<vector<string>> tracker_info){
    bool connection_established = false;
    for(int i = 0;i<(int)tracker_info.size();i++){
        sockaddr_in tracker_address;
        tracker_address.sin_family = AF_INET;
        tracker_address.sin_port = htons(stoi(tracker_info[i][1]));
        tracker_address.sin_addr.s_addr = inet_addr(&tracker_info[i][0][0]);
        if(connect(client_socket,(struct sockaddr*)&tracker_address, sizeof(tracker_address))!=-1){
            connection_established = true;
            cout<<"Connected to Tracker: "<<tracker_info[i][0]<<":"<<tracker_info[i][1]<<"\n";
            break;
        };
    }

    if(!connection_established){
        cout<<"Trackers are not online!!\n";
        close(client_socket);
        exit(0);
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

void handle_input(string address, int port, int client_socket, vector<vector<string>> tracker_info){
    vector<thread> threads;
    while(true){
        string input;
        getline(cin,input);

        vector<string> SHA1;
        vector<string> tokens = tokenizer(input);
        if(input.find("logout")!=string::npos && logged_in){
            ssize_t bytes_sent = send(client_socket, &input[0], input.size(), 0);
            cout<<"Successfully Logged Out!!!\n";
            logged_in = false;
            exit(0);
        }
        if((input.find("create_user")== string::npos && input.find("login")== string::npos) && !logged_in){
            cout<<"Please login first!!!\n";
            continue;
        }
        if(input.find("login")!=string::npos){
            input = input + " " + address + ":" + to_string(port);
        }
        if(input.find("upload_file")!= string::npos){
            string file_path = tokens[1];
            size_t pos = file_path.find_last_of("/\\");
            string file_name;
            
            if (pos != string::npos) {
                file_name = file_path.substr(pos + 1);
            }else{
                file_name = file_path;
            }
            file_name_to_path[file_name] = file_path;

            SHA1 = SHA1_hash_function(tokens);
            for(int i =0;i<(int)SHA1.size();i++){
                input = input + " " + SHA1[i] + " ";
            }
            leeching_tracker[file_name] = vector<bool> (SHA1.size());
            threads.push_back(thread(receive_requests, address, port));
            threads.back().detach();
        }

        ssize_t bytes_sent = send(client_socket, &input[0], input.size(), 0);
        
        if (bytes_sent == -1) {
            cout << "Send failed.\n";
            cout<<"Tracker might have disconnected.\n";
            close(client_socket);
            exit(0);
        }
    }
}

void run_client(string address, int port, vector<vector<string>> tracker_info){
    int client_socket = socket(AF_INET, SOCK_STREAM,0);

    connect_tracker(client_socket, tracker_info);

    thread handle_input_thread(handle_input,address, port, client_socket, tracker_info);
    thread handle_response_from_tracker_thread(handle_response_from_tracker, client_socket, address, port);
    
    handle_input_thread.join();
    handle_response_from_tracker_thread.join();

    cout<<"\nclient socket closed\n";
    close(client_socket);
}

int main(int argc, char* argv[]){

    if(argc<3){
        cout<<"2 arguments are required!!\n";
        exit(0);
    }

    string client_address_and_port = argv[1];

    string client_address;
    int port;

    unsigned long int partition;

    for(unsigned long int i =0;i<client_address_and_port.size();i++){
        if(client_address_and_port[i]==':'){
            partition = i;
            break;
        }
    }

    client_address = client_address_and_port.substr(0,partition);
    port = stoi(client_address_and_port.substr(partition+1));

    string tracker_info_filename = argv[2];

    int fd = open(&tracker_info_filename[0], O_RDONLY);
    if (fd == -1) {
        cout<<"Error opening file\n";
        exit(0);
    }

    vector<vector<string>> tracker_info;

    char buffer[1024];
    ssize_t bytes_read;
    string tracker_address_and_port="";

    while((bytes_read = read(fd,buffer, 1024))>0){
        
        for(int i =0;i<bytes_read;i++){
            char c = buffer[i];
            if(c=='\n'||c=='\0'){
                for(int j =0;j<(int)tracker_address_and_port.size();j++){
                    if(client_address_and_port[j]==':'){
                        partition = j;
                        break;
                    }
                }
                string temp_address = tracker_address_and_port.substr(0,partition);
                string temp_port = tracker_address_and_port.substr(partition+1,4);

                tracker_info.push_back({temp_address,temp_port});

                tracker_address_and_port = "";
            }else{
                tracker_address_and_port += c;
            }
        }
    }

    if (!tracker_address_and_port.empty()) {
        size_t partition = tracker_address_and_port.find(':');
        if (partition != string::npos) {
            string temp_address = tracker_address_and_port.substr(0, partition);
            string temp_port = tracker_address_and_port.substr(partition + 1);

            tracker_info.push_back({temp_address, temp_port});
        }
    }

    close(fd);

    run_client(client_address,port,tracker_info);

    return 0;
}