#include "log_macros.h"
#include "open_browser.h"

#include "spotify_api/spotify_api.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

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

boost::posix_time::ptime get_ptime_from_8601(std::string input)
{
    boost::remove_erase_if(input, boost::is_any_of(":-"));
    return boost::posix_time::from_iso_string(input);
}

int main(int argc, char** argv)
{
    auto client_id = std::string{};
    auto client_secret = std::string{};
    auto access_token = std::string{};
    auto refresh_token = std::string{};
    auto source_playlist_url = std::string{};
    auto target_playlist_uri = std::string{};
    auto backup_file = std::string{};
    auto username = std::string{};
    auto filter_months = 0;
    
    auto options = po::options_description{"Allowed options"};
    options.add_options()
    ("help", "produce help message")
    ("source-playlist-url", po::value(&source_playlist_url)->required(), "source spotify playlist url")
    ("target-playlist-uri", po::value(&target_playlist_uri)->required(), "target spotify playlist uri")
    ("filter-months", po::value(&filter_months)->required(), "recent playlist months")
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
    
    auto playlist = session.get_playlist(source_playlist_url);
    
    auto cutoff_time = boost::posix_time::second_clock::universal_time() - boost::gregorian::months(filter_months);
    
    auto tracks = std::vector<std::string>{};
    boost::push_back(tracks, playlist.tracks
                     | boost::adaptors::reversed
                     | boost::adaptors::filtered(
                                                 [cutoff_time](auto const& track)
                                                 {
                                                     return !boost::algorithm::starts_with(track.uri, "spotify:local") && get_ptime_from_8601(track.added_at) > cutoff_time;
                                                 })
                     | boost::adaptors::transformed([](auto const& track) { return track.uri; }));
    
    session.set_playlist_tracks(target_playlist_uri, tracks);
}
