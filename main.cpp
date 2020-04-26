#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cassert>
#include <curl/curl.h>

using namespace std;
namespace fs = filesystem;

/******global vars******/
string save_folder;
vector<string> prased_urls, failed_urls;
vector<string> imgs, other, links, medias;
string server;
/**********************/
void read_file_into(string file_path, string &target){
	fstream input_file;
	input_file.open(file_path);
	if(!input_file.is_open()) throw("Failes to open file\n");
	else{
		stringstream input;
		input << input_file.rdbuf();
		target = input.str();
	}
}
string trim( string &str ){
	while(isspace(str[str.length()-1])) str.pop_back();
	while(isspace(str[0]) ) str.erase(str.begin());
	return str;
}
bool compare_string(string str1, string str2, bool clear_trail_space = true){
	if(str1.compare(str2) == 0){
		return true;
	}
	if(clear_trail_space == true){
		trim(str1); trim(str2);
	}
	if(str1.size() != str2.size()){
		return false;
	}
	auto char_cmp = [](const char ch1, const char ch2){
		return ( tolower(ch1) == tolower(ch2) );
	};
	return equal(str1.begin(), str1.end(), str2.begin(), char_cmp);
}
string get_dir( string src ){
	trim(src);
	if(src.back() == '/'){
		src.pop_back();
	}
	string::size_type last_slash = src.rfind("/");
	string::size_type last_dot = src.rfind(".");
	if(last_dot > last_slash){ // path with filename eg: http://localhost/web/index.php
		src.erase(last_slash);
	} else { //path without filename eg: http://localhost/home
	}
	return src;
}
string get_parent_dir( string src ){ // eg: /home/sudip/Documents/Projects/main.cpp
	src = get_dir( src ); // now: /home/sudip/Documents/Projects
	string::size_type second_last_slash = src.rfind("/");
	if(second_last_slash != string::npos) src.erase(second_last_slash);
	return src;
}
int make_dir( string path ){
	path = trim(path);
	if(path.back() == '/') path.pop_back();
	if(path[0] != '/'){
		path = fs::current_path().string()+'/'+path;
	}
	if( fs::exists(path) ){ //check if dir already exist
		// std::cout << "\nAlready exists";
		return 1;
	}
	// home/css/private/last/
	string::size_type found_at = path.find("/", path.find("/")+1); //skip first / as it gves empty strng
	string pos = path.substr(0, path.find("/", found_at));
	while( true ){
		pos = trim(pos);
		if( !fs::exists(pos) ){
			fs::create_directory(pos);
			// std::cout << "\n" << pos << " [created]";
		} else {
			// cout << "\n" << pos << " [exist] ";
		}
		if(found_at == string::npos) break;
		found_at = path.find("/", found_at+1);
		pos = path.substr(0, found_at);
	}
	return 0;
	
}
class Prase{
public:
	static string find_protocol( const string &url ){
		return  url.substr( 0,
			url.find("://")
		);
	}
	static string find_path( const string &url , const string &protocol, const string &domain ){
		string tmp = url.substr(
			// after domain to end
			protocol.length() + 3  + domain.length() // this is index to start
		);
		return tmp;
	}
	static string find_domain( const string &url, const string &protocol ){
		int start = protocol.length()+3; // first [ / ] after [ :// ] is starting od domain
		int len = url.find("/", start)-start; //get length of domain as as second param of substr is len not index
		return url.substr( start, len );
	}
	static string find_domain( const string &url ){ // refrence to find from domain only
		return find_domain(url, find_protocol(url));
	}
	static string find_path( const string &url ){
		return find_path(
			url, find_protocol(url), find_domain(url)
		);
	}
	static string find_file_name( const string &path){
		string name = path;
		if(name.back() == '/') name.pop_back();
		string::size_type last_slash = name.rfind("/");
		if(last_slash != string::npos){
			name = name.substr(last_slash+1); // skip [ / ] itself
		}
		return name;
	}
	static string find_host( const string &domain ){
		// www.me.people.com;
		return domain.substr(
			//is from second-last . to end
			domain.find(".", (domain.find("."))+1 )+1 //skiip the dot
		);
	}
	
private:
	bool prase_url(string &url, string current_page){
		string current_dir = get_dir(current_page);
		if(url.size() <= 0){ // checkk for empty url
			return false;
		} else if(trim(url).size() <= 0){
			return false;
		}
		if(url[0] == '#'){ //avoid internal linkk
			return false;
		}
		if(url[0] == '.' || url[0] == '/'){
			string path = current_dir;
			if( url[0] == '.' ) url = ( server+current_dir + "/" + url );
			else if(url[0] == '/') url = ( server + url );
		}
		bool is_internal = true;
		{
			if( domain.compare(find_domain(url)) != 0 ) //compare to domain of input
				is_internal = false;
		}
		if(is_internal == false){
			return true;
		}
		//remove dot dir path with absolute path
		string::size_type found_at = url.find("/./");
		while(found_at != string::npos){
			url.replace(found_at, 3, "/");
			found_at = url.find("/./", found_at+1);
		}
		found_at = url.find("/../");
		while(found_at != string::npos){
			int start = found_at-1;
			string to_rep;
			int i;
			for( i = start; url[i] != '/'; --i ) to_rep.push_back(url[i]);
			url.replace(i, to_rep.length()+4, "");
			found_at = url.find("/../", found_at+1);
		}
		
		string file_extension;
		{
			string::size_type ext_in = url.rfind("."); // find last .
			string::size_type dir_in = url.rfind("/"); // find last /
			if(
				ext_in != string::npos && dir_in != string::npos &&
				ext_in > dir_in
			){
				/* format should be like google.com/img.png
				 * not like google.com/maps so last [ . ] be later than last [ / ]
				 */
				file_extension = url.substr(ext_in+1); // sip the . itself
			}
		}
		
		if(file_extension.length() > 0){
			if(
				compare_string(file_extension, "png") || compare_string(file_extension, "jpg") ||
				compare_string(file_extension, "jpeg") || compare_string(file_extension, "gif") ||
				compare_string(file_extension, "svg") || compare_string(file_extension, "tif")
			){
				if( find(imgs.begin(), imgs.end(), url) == imgs.end() ) //enter if not already present
					imgs.push_back(url);
			} else if(
				compare_string(file_extension, "mp4") || compare_string(file_extension, "mp3") ||
				compare_string(file_extension, "mov") || compare_string(file_extension, "wav") ||
				compare_string(file_extension, "aif") || compare_string(file_extension, "mpeg") ||
				compare_string(file_extension, "flac")
			){
				if( find(medias.begin(), medias.end(), url) == medias.end() )
					medias.push_back(url);
			} else if(
				compare_string(file_extension, "html") || compare_string(file_extension, "htm") ||
				compare_string(file_extension, "php") || compare_string(file_extension, "asp") ||
				compare_string(file_extension, "website") || compare_string(file_extension, "aspx") ||
				compare_string(file_extension, "ashx") || compare_string(file_extension, "dhtml") ||
				compare_string(file_extension, "htmls") || compare_string(file_extension, "jsp") ||
				compare_string(file_extension, "shtml") || compare_string(file_extension, "cgi")
			){
				if( 
					find(links.begin(), links.end(), url) == links.end() &&
					find(prased_urls.begin(), prased_urls.end(), url) == prased_urls.end()
				){
					links.push_back(url);
				}
			} else{
				if( find(other.begin(), other.end(), url) == other.end() )
					other.push_back(url);
			}
		}
		else{
			if(url.back() == '/') url.pop_back(); // remove last [ / ]
			// if nothing possibly that may be line eg: href = "../contact" -> "../contact/index.html"
			CURL *curl;
			curl = curl_easy_init();
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
			string possib_exts[] { ".php", ".cgi", ".html", ".asp"};
			for( int i = 0; i < 4; i++ ){
				curl_easy_setopt(curl, CURLOPT_URL, (url+"/index"+possib_exts[i]).c_str());
				if( curl_easy_perform(curl) == CURLE_OK ){
					long response_code;
					curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
					if( response_code != 200 ){
						continue;
					} else if( response_code == 200 ){
						url = url + "/index" + possib_exts[i];
						break;
					}
				} else {
					continue;
				}
			}
			curl_easy_cleanup(curl);
			
			if( 
				find(links.begin(), links.end(), url) == links.end() &&
				find(prased_urls.begin(), prased_urls.end(), url) == prased_urls.end()
			){
				links.push_back(url);
			}
		}
		return true;
	}
	
	void find_links(string &file_content, string main_page){
		std::vector<long int> link_indexes;
		string::size_type last_found = 0;
		for(int i = 0;  static_cast<unsigned long int>(i) < file_content.length(); ++i){
			string::size_type in = file_content.find("src", last_found+1);
			if( in == string::npos ){
				break;
			}
			last_found = in;
			link_indexes.push_back(in+3); // skip word src;
		}
		last_found = 0;
		for(int i = 0;  last_found < file_content.length(); ++i){
			string::size_type in = file_content.find("href", last_found+1);
			if(in == string::npos){
				break;
			}
			last_found = in;
			link_indexes.push_back(in+4); // skip word src;
		}
		for(unsigned long int i = 0; i < link_indexes.size(); i++){
			long int pos = link_indexes[i];
			//skip whitespace before = and = and space
			while( isspace(file_content[pos]) || file_content[pos] == '=' ) pos++;
			char quote_type = file_content[pos];
			pos++; //skip quote_type
			string url;
			while(true){
				if( file_content[pos] == quote_type || file_content[pos] == '>' ){
					break;
				}
				url.push_back(file_content[pos]);
				pos++;
			}
			prase_url( url, main_page );
		}
	}
public:
	string protocol, path, domain, host, sub_domain;
	static size_t write_call_back(void *contents, size_t size, size_t nmemb, void *userp)
	{
		((std::string*)userp)->append((char*)contents, size * nmemb);
		return size * nmemb;
	}
	int init(string url){
		std::cout << "\n*** INITILIAED WITH -> " << url;
		string file_content;
		CURL *curl;
		int res = 0;
		
		
		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_call_back);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file_content);
		res = curl_easy_perform(curl);
		
		if( res != CURLE_OK ){
			std::cout << "\nFailed with A curl Error..";
			failed_urls.push_back(url);
			links.erase(links.begin());
			return -1;
		}
		// cout << "\n******\n" << file_content << "*****";
		curl_easy_cleanup(curl);
		protocol= find_protocol( url );
		domain	= find_domain(url, protocol);
		path 	= find_path(url, protocol, domain);
		host 	= find_host(domain);
		try{
			find_links(file_content, find_path(url));
		} catch( ... ){
			std::cout << "\n\n\n\n******FAILED TO PRASE********" << url << "\n\n\n\n\n";
			failed_urls.push_back(url);
			links.erase(links.begin());
			return 1;
		}
		prased_urls.push_back(url);
		links.erase(links.begin());
		return 0;
	}
	
} main_page;

int main(int, char** argv ) {
	save_folder = argv[2];
	string input_url = argv[1];
	
	if( !fs::exists(save_folder) ) assert("\nSave folder do not exist\n");
	fs::current_path(save_folder);
	
	server = Prase::find_protocol(input_url) + "://" +Prase::find_domain(input_url);
	std::cout << "\n**SERVER SET TO " << server;
	
	links.push_back(trim(input_url));
	main_page.init(input_url);
	while( true ){
		if( links.size() == 0 ) break;
		Prase next;
		next.init( links.front() );
		if( links.size() == 0 ) break;
	}
	/*{ //print
		cout << "\n\t****SHOWING RES*****";
		cout << "\n IMAGES";
		for(string s : imgs) cout << "\n\t" << s;
		cout << "\n MEDIAS";
		for(string s : medias) cout << "\n\t" << s;
		cout << "\n OTHERS";
		for(string s : other) cout << "\n\t" << s;
		cout << "\n Praseable LINKS";
		for(string s : prased_urls) cout << "\n\t" << s;
	}*/
	
	CURL *curl = curl_easy_init();
	FILE *file;
	long int res;
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	auto download = [&](string url) -> bool{
		string to_store_path = save_folder + get_dir( Prase::find_path(url) );
		make_dir(to_store_path);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		string file_name =  ( to_store_path + "/" + Prase::find_file_name(Prase::find_path(url)) );
		cout << file_name;
		file = fopen(file_name.c_str(), "wb");
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
		res = curl_easy_perform(curl);
		fclose(file);
		if( res == CURLE_OK ){
			cout << "\n [ downloaded ] " << url;
			return true;
		} else {
			cout << "\n [ failed ] " << url;
			return false;
		}
	};
	{ //download
		for(string s: imgs){
			cout << "\nDownloading " << s << " -> ";
			download(s);
		}
		for(string s: medias){
			cout << "\nDownloading " << s << " -> ";
			download(s);
		}
		for(string s: prased_urls){
			cout << "\nDownloading " << s << " -> ";
			download(s);
		}
		for(string s: other){
			cout << "\nDownloading " << s << " -> ";
			download(s);
		}
	}
	
	
	std::cout << std::endl;
	return 0;
}
