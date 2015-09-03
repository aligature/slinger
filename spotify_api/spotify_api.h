//
//  spotify_api.h
//  slinger
//
//  Created by Andy Webber on 9/2/15.
//  Copyright (c) 2015 Andy Webber. All rights reserved.
//

#pragma once

#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>

#include <boost/filesystem.hpp>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <memory>

namespace spotify { namespace api {
    
    using namespace std::literals;
    
    using namespace pplx;
    using namespace web;
    using namespace web::http;
    using namespace web::http::client;
    using namespace web::http::experimental::listener;
    using namespace web::http::oauth2::experimental;
    
    struct Track
    {
        std::string name;
        std::string uri;
        std::string album;
        using Artists = std::vector<std::string>;
        Artists artists;
    };
    
    struct PlaylistTrack: Track
    {
        std::string added_at;
    };
    
    struct Playlist
    {
        std::string name;
        std::string uri;
        using Tracks = std::vector<PlaylistTrack>;
        Tracks tracks;
    };
    
    class Session
    {
    public:
        Session() {}
        
        Session(std::string username)
            : username_(username)
        {
            config_.load();
        }
        
        Session(std::string username, std::string client_id, std::string client_secret)
            : username_(username)
            , config_{client_id, client_secret}
        {
            config_.load();
        }
        
        Session(Session const&) = delete;
        Session& operator=(Session const&) = delete;
        Session(Session&&) = default;
        Session& operator=(Session&&) = default;
        
        bool open();
        Playlist get_playlist(std::string uri);
        Playlist get_playlist(json::value& value);
        Playlist::Tracks get_playlist_page(std::string uri);
        Playlist::Tracks get_playlist_page(json::value& value);
        Playlist::Tracks get_playlist_tracks(json::value& value);
        
        struct Config
        {
            std::string client_id;
            std::string client_secret;
            
            struct Tokens
            {
                std::string access_token;
                std::string refresh_token;
            };
            using UserTokens = std::map<std::string, Tokens>;
            UserTokens user_tokens;
            
            static std::string config_file();
            
            void load();
            void save();
            
            bool valid() const { return !client_id.empty() && !client_secret.empty(); }
        };
        
        
    private:
        oauth2_config authenticate();
        
        std::string username_;
        
        Config config_;
        
        std::unique_ptr<http_client> client_;
    };
    
    inline std::string Session::Config::config_file()
    {
        struct passwd *pw = getpwuid(getuid());
        char const* homedir = pw->pw_dir;
        auto path = boost::filesystem::path{homedir};
        path /= ".slinger_config";
        return path.native();
    }
    
    inline void Session::Config::load()
    {
        auto file = std::ifstream{config_file()};
        
        auto json = json::value{};
        try
        {
            file >> json;
        }
        catch(...)
        {
        }
        
        if(!json.is_null())
        {
            client_id = json["client_id"].as_string();
            client_secret = json["client_secret"].as_string();
            for(auto& element: json["user_tokens"].as_array())
            {
                user_tokens[element["username"].as_string()] = {element["access_token"].as_string(), element["refresh_token"].as_string()};
            }
        }
    }
    
    inline void Session::Config::save()
    {
        auto file = std::ofstream{config_file()};
        
        auto json = json::value{};
        json["client_id"] = json::value(client_id);
        json["client_secret"] = json::value(client_secret);
        
        auto token_config = json::value{};
        auto i = 0;
        for(auto const& value: user_tokens)
        {
            auto& token = token_config[i];
            token["username"] = json::value(value.first);
            token["access_token"] = json::value(value.second.access_token);
            token["refresh_token"] = json::value(value.second.refresh_token);
            ++i;
        }
        json["user_tokens"] = token_config;
        
        file << json;
    }
    
    oauth2_config Session::authenticate()
    {
        auto redirect_uri = "http://localhost:5001";
        auto oauth = oauth2_config{config_.client_id
            , config_.client_secret
            , "https://accounts.spotify.com/authorize"
            , "https://accounts.spotify.com/api/token"
            , redirect_uri};
        
        auto scopes = { "playlist-read-private"s
            , "playlist-read-collaborative"s
            , "playlist-modify-public"s
            , "playlist-modify-private"s
            , "user-library-read user-library-modify"s};
        
        oauth.set_scope(boost::algorithm::join(scopes, " "));
        
        auto auth_uri = oauth.build_authorization_uri(true);
        
        auto auth_event = task_completion_event<void>{};
        auto auth_task = task<void>{auth_event};
        auto token = oauth2_token{};
        
        auto& user_tokens = config_.user_tokens[username_];
        if(user_tokens.access_token.empty())
        {
            auto listener = http_listener{redirect_uri};
            listener.support(
                             [&](auto request)
                             {
                                 auto uri = request.request_uri();
                                 oauth.token_from_redirected_uri(uri).wait();
                                 token = oauth.token();
                                 request.reply(status_codes::OK, "welcome to using slinger. enjoy.").wait();
                                 auth_event.set();
                             });
            listener.open().wait();
            
            open_browser(auth_uri);
            auth_task.wait();
            
            auto const& token = oauth.token();
            user_tokens.access_token = token.access_token();
            user_tokens.refresh_token = token.refresh_token();
        }
        else
        {
            token.set_access_token(user_tokens.access_token);
            token.set_refresh_token(user_tokens.refresh_token);
            oauth.set_token(token);
            oauth.token_from_refresh().wait();
        }
        
        config_.save();
        return oauth;
    }
    
    bool Session::open()
    {
        if(!config_.valid())
            return false;
        
        auto oauth = authenticate();
        
        auto base_uri = uri_builder{"https://api.spotify.com/"};
        //auto playlist_query = base_uri;
        //playlist_query.append_path(playlist_url).append_query("market", "US");
        
        auto client_config = http_client_config{};
        client_config.set_oauth2(oauth);
        client_ = std::make_unique<http_client>(base_uri.to_uri(), client_config);
        
        return true;
    }
    
    Playlist Session::get_playlist(std::string url)
    {
        auto response = client_->request(methods::GET, url).get();
        response.content_ready().wait();
        auto x = response.to_string();
        auto json = response.extract_json().get();
        
        return get_playlist(json);
    }
    
    Playlist Session::get_playlist(json::value& json)
    {
        auto playlist = Playlist{};
        
        playlist.name = json["name"].as_string();
        //output << playlist_name << "\n";
        //output << "track;album;artists;added at;uri\n";
        
        playlist.tracks = get_playlist_page(json["tracks"]);
        
        //jauto num = save_playlist_page(client, page, output);
        //jLOG_INFO << str(boost::format{"saved playlist \"%s\" with %s tracks"} % playlist_name % num);
        //jreturn num;
        
        return playlist;
    }
    
    Playlist::Tracks Session::get_playlist_page(std::string url)
    {
        auto response = client_->request(methods::GET, url).get();
        response.content_ready().wait();
        auto json = response.extract_json().get();
        
        return get_playlist_page(json);
    }
    
    Playlist::Tracks Session::get_playlist_page(json::value& json)
    {
        auto tracks = get_playlist_tracks(json);
        
        auto& next = json["next"];
        if(!next.is_null())
        {
            auto next_tracks = get_playlist_page(next.as_string());
            tracks.insert(tracks.end(), std::make_move_iterator(std::begin(next_tracks)), std::make_move_iterator(std::end(next_tracks)));
        }
        
        return tracks;
    }
    
    Playlist::Tracks Session::get_playlist_tracks(json::value& json)
    {
        auto tracks = Playlist::Tracks{};
        
        auto& json_tracks = json["items"].as_array();
        for(auto& playlist_track: json_tracks)
        {
            auto& json_track = playlist_track["track"];
            auto const& added_at = playlist_track["added_at"].as_string();
            auto const& album_name = json_track["album"]["name"].as_string();
            auto const& track_name = json_track["name"].as_string();
            auto const& uri = json_track["uri"].as_string();
            
            auto track = PlaylistTrack{};
            track.name = track_name;
            track.uri = uri;
            track.album = album_name;
            track.added_at = added_at;
            
            auto& artists = json_track["artists"].as_array();
            for(auto& artist: artists)
            {
                track.artists.emplace_back(artist["name"].as_string());
            }
            tracks.emplace_back(std::move(track));
            
            //auto artists_string = boost::algorithm::join(artist_names, ",");
            //output << boost::algorithm::join(string_vec{quote(track_name), quote(album_name), quote(artists_string), added_at, uri}, ",") << "\n";
        }
        
        return tracks;
    }
    
//    template<typename Stream>
//    size_t save_playlist_page(http_client& client, json::value& page, Stream& output)
//    {
//        auto num = size_t{0};
//        
//        auto& tracks = page["items"].as_array();
//        for(auto& playlist_track: tracks)
//        {
//            ++num;
//            auto& track = playlist_track["track"];
//            auto const& added_at = playlist_track["added_at"].as_string();
//            auto const& album_name = track["album"]["name"].as_string();
//            auto const& track_name = track["name"].as_string();
//            auto& artists = track["artists"].as_array();
//            auto const& uri = track["uri"].as_string();
//            auto artist_names = string_vec{};
//            for(auto& artist: artists)
//            {
//                artist_names.emplace_back(artist["name"].as_string());
//            }
//            auto artists_string = boost::algorithm::join(artist_names, ",");
//            output << boost::algorithm::join(string_vec{quote(track_name), quote(album_name), quote(artists_string), added_at, uri}, ",") << "\n";
//        }
//        
//        auto& next = page["next"];
//        if(!next.is_null())
//        {
//            auto response = client.request(methods::GET, next.as_string()).get();
//            response.content_ready().wait();
//            auto json = response.extract_json().get();
//            num += save_playlist_page(client, json, output);
//        }
//        
//        return num;
//    }
//    
//    template<typename Stream>
//    size_t save_playlist(http_client& client, uri playlist_uri, Stream& output)
//    {
//    }
    
    
}
}