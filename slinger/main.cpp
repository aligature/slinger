#include "log_macros.h"
#include "open_browser.h"

#include "spotify_api/spotify_api.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>


namespace po = boost::program_options;

using string_vec = std::vector<std::string>;

std::string quote(std::string const& in)
{
    return "\"" + in + "\"";
}



int main(int argc, char** argv)
{
    auto client_id = std::string{};
    auto client_secret = std::string{};
    auto access_token = std::string{};
    auto refresh_token = std::string{};
    auto playlist_url = std::string{};
    auto backup_file = std::string{};
    auto username = std::string{};
    
    auto options = po::options_description{"Allowed options"};
    options.add_options()
    ("help", "produce help message")
    ("playlist-url", po::value(&playlist_url)->required(), "spotify playlist url")
    ("username", po::value(&username)->required(), "spotify username")
    ("backup-file", po::value(&backup_file), "backup output file")
    
    ("client-id", po::value(&client_id), "spotify api client id")
    ("client-secret", po::value(&client_secret), "spotify api client secret")
    ;
    
    try
    {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, options), vm);
        po::notify(vm);    
        
        if (vm.count("help"))
        {
            LOG_ERROR << options;
            return 1;
        }
    }
    catch(std::exception const& e)
    {
        LOG_ERROR << "Error: " << e.what();
        return 1;
    }
    
    auto session = spotify::api::Session{};
    
    if(!client_id.empty() || !client_secret.empty())
    {
        session = spotify::api::Session{username, client_id, client_secret};
    }
    else
    {
        session = spotify::api::Session{username};
    }
    session.open();
    
    auto playlist = session.get_playlist(playlist_url);
    
    std::cout << "got it" << std::endl;
    
        //output << playlist_name << "\n";
        //output << "track;album;artists;added at;uri\n";
            
            //auto artists_string = boost::algorithm::join(artist_names, ",");
            //output << boost::algorithm::join(string_vec{quote(track_name), quote(album_name), quote(artists_string), added_at, uri}, ",") << "\n";
    
//
//    if(!config.valid())
//    {
//        LOG_ERROR << "Error: supply client id and secret";
//        return 1;
//    }
    
    
    //auto output = std::ofstream{backup_file};
    //save_playlist(client, playlist_query.to_uri(), output);
}
