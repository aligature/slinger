#include "log_macros.h"
#include "open_browser.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>


using namespace std::literals;

using namespace pplx;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;
using namespace web::http::oauth2::experimental;
namespace po = boost::program_options;

using string_vec = std::vector<std::string>;

std::string quote(std::string const& in)
{
   return "\"" + in + "\"";
}

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

   std::string config_file()
   {
      struct passwd *pw = getpwuid(getuid());
      char const* homedir = pw->pw_dir;
      auto path = boost::filesystem::path{homedir};
      path /= ".slinger_config";
      return path.native();
   }

   void load()
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

   void save()
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

   bool valid() const
   {
      return !client_id.empty() && !client_secret.empty();
   }
};

oauth2_config authenticate(Config& config, std::string const& username)
{
   auto redirect_uri = "http://localhost:5001";
   auto oauth_config = oauth2_config{config.client_id
         , config.client_secret
         , "https://accounts.spotify.com/authorize"
         , "https://accounts.spotify.com/api/token"
         , redirect_uri};

   auto scopes = { "playlist-read-private"s
      , "playlist-read-collaborative"s
         , "playlist-modify-public"s
         , "playlist-modify-private"s
         , "user-library-read user-library-modify"s};

   oauth_config.set_scope(boost::algorithm::join(scopes, " "));

   auto auth_uri = oauth_config.build_authorization_uri(true);

   auto auth_event = task_completion_event<void>{};
   auto auth_task = task<void>{auth_event};
   auto token = oauth2_token{};

   auto& user_tokens = config.user_tokens[username];
   if(user_tokens.access_token.empty())
   {
      auto listener = http_listener{redirect_uri};
      listener.support(
            [&](auto request)
            {
            auto uri = request.request_uri();
            oauth_config.token_from_redirected_uri(uri).wait();
            token = oauth_config.token();
            request.reply(status_codes::OK, "welcome to using slinger. enjoy.").wait();
            auth_event.set();
            });
      listener.open().wait();

      open_browser(auth_uri);
      auth_task.wait();

      auto const& token = oauth_config.token();
      user_tokens.access_token = token.access_token();
      user_tokens.refresh_token = token.refresh_token();
   }
   else
   {
      token.set_access_token(user_tokens.access_token);
      token.set_refresh_token(user_tokens.refresh_token);
      oauth_config.set_token(token);
      oauth_config.token_from_refresh().wait();
   }

   config.save();
   return oauth_config;
}

template<typename Stream>
size_t save_playlist_page(http_client& client, json::value& page, Stream& output)
{
   auto num = size_t{0};

   auto& tracks = page["items"].as_array();
   for(auto& playlist_track: tracks)
   {
      ++num;
      auto& track = playlist_track["track"];
      auto const& added_at = playlist_track["added_at"].as_string();
      auto const& album_name = track["album"]["name"].as_string();
      auto const& track_name = track["name"].as_string();
      auto& artists = track["artists"].as_array();
      auto const& uri = track["uri"].as_string();
      auto artist_names = string_vec{};
      for(auto& artist: artists)
      {
         artist_names.emplace_back(artist["name"].as_string());
      }
      auto artists_string = boost::algorithm::join(artist_names, ",");
      output << boost::algorithm::join(string_vec{quote(track_name), quote(album_name), quote(artists_string), added_at, uri}, ",") << "\n";
   }

   auto& next = page["next"];
   if(!next.is_null())
   {
      auto response = client.request(methods::GET, next.as_string()).get();
      response.content_ready().wait();
      auto json = response.extract_json().get();
      num += save_playlist_page(client, json, output);
   }

   return num;
}

template<typename Stream>
size_t save_playlist(http_client& client, uri playlist_uri, Stream& output)
{
   auto response = client.request(methods::GET, playlist_uri.to_string()).get();
   response.content_ready().wait();

   auto json = response.extract_json().get();

   auto& playlist_name = json["name"].as_string();
   output << playlist_name << "\n";
   output << "track;album;artists;added at;uri\n";

   auto& page = json["tracks"];

   auto num = save_playlist_page(client, page, output);
   LOG_INFO << str(boost::format{"saved playlist \"%s\" with %s tracks"} % playlist_name % num);
   return num;
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

   auto config = Config{};
   config.load();

   if(!client_id.empty())
      config.client_id = client_id;
   if(!client_secret.empty())
      config.client_secret = client_secret;

   if(!config.valid())
   {
      LOG_ERROR << "Error: supply client id and secret";
      return 1;
   }

   auto oauth_config = authenticate(config, username);

   auto base_uri = uri_builder{"https://api.spotify.com/"};
   auto playlist_query = base_uri;
   playlist_query.append_path(playlist_url).append_query("market", "US");

   auto client_config = http_client_config{};
   client_config.set_oauth2(oauth_config);
   auto client = http_client{base_uri.to_uri(), client_config};

   auto output = std::ofstream{backup_file};
   save_playlist(client, playlist_query.to_uri(), output);
}
