#include "log_macros.h"
#include "open_browser.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>

#include <iostream>
#include <string>

using namespace std::literals;

using namespace pplx;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;
using namespace web::http::oauth2::experimental;
namespace po = boost::program_options;


int main(int argc, char** argv)
{
   auto client_id = std::string{};
   auto client_secret = std::string{};
   auto access_token = std::string{};
   auto refresh_token = std::string{};
   auto username = std::string{};
   auto playlist = std::string{};

   auto options = po::options_description{"Allowed options"};
   options.add_options()
      ("help", "produce help message")
      ("client-id", po::value(&client_id)->required(), "spotify api client id")
      ("client-secret", po::value(&client_secret)->required(), "spotify api client secret")
      ("username", po::value(&username)->required(), "spotify username")
      ("playlist", po::value(&playlist)->required(), "spotify playlist")
      ("access-token", po::value(&access_token), "saved access token")
      ("refresh-token", po::value(&refresh_token), "saved refresh token")
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

   auto redirect_uri = "http://localhost:5001";
   auto oauth_config = oauth2_config{client_id
         , client_secret
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
   LOG_INFO << "listening on: " << redirect_uri;

   auto auth_event = task_completion_event<void>{};
   auto auth_task = task<void>{auth_event};
   auto token = oauth2_token{};

   if(access_token.empty())
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
   }
   else
   {
      token.set_access_token(access_token);
      token.set_refresh_token(refresh_token);
      oauth_config.set_token(token);
      oauth_config.token_from_refresh().wait();
   }

   LOG_INFO << "access: " << token.access_token();
   LOG_INFO << "refresh: " << token.refresh_token();
   LOG_INFO << "expires: " << token.expires_in();

   auto base_uri = uri_builder{"https://api.spotify.com/"};
   auto post_query = base_uri;
   post_query.append_path("v1").append_path("users").append_path(username).append_path("playlists").append_path(playlist).append_query("market", "US");
   LOG_DEBUG << post_query.to_string();

   auto client_config = http_client_config{};
   client_config.set_oauth2(oauth_config);
   auto client = http_client{base_uri.to_uri(), client_config};
   auto response = client.request(methods::GET, post_query.to_string()).get();
   response.content_ready().wait();
   LOG_DEBUG << response.to_string().size();
   //auto json = response.extract_json().get();
   //std::cout << json["external_urls"]["spotify"] << std::endl;

}
