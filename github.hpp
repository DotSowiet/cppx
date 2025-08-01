#pragma once

#include <curl/curl.h> // Requires libcurl to be installed
#include <iostream>
#include <nlohmann/json.hpp> // Requires nlohmann/json to be installed
#include <string>

// Definition of the structure to store GitHub repository information
struct GithubInfo
{
    std::string name;             // Repository name
    std::string description;      // Repository description
    int stars;                    // Number of stars
    int forks;                    // Number of forks
    int open_issues;              // Number of open issues
    std::string last_commit_date; // Date of the last commit
    std::string html_url;         // GitHub repository URL
    bool success;                 // Whether information retrieval was successful
    std::string error_message;    // Error message (if any)
};

// cURL callback to write data to a string
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

// Function to retrieve GitHub repository information
GithubInfo getRepoInfo(const std::string &owner, const std::string &repo_name)
{
    GithubInfo info;
    info.success = false; // Default to failure

    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl)
    {
        std::string url = "https://api.github.com/repos/" + owner + "/" + repo_name;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        // Setting User-Agent is required by GitHub API
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "C++ Github Repo Info Fetcher");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        // Optional: enable redirection following
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            info.error_message = "curl_easy_perform() failed: " + std::string(curl_easy_strerror(res));
        }
        else
        {
            try
            {
                // Parsing JSON response
                auto json_data = nlohmann::json::parse(readBuffer);

                // Check if the API returned an error (e.g., repository not found)
                if (json_data.contains("message") && json_data["message"] == "Not Found")
                {
                    info.error_message = "Repository not found or access denied.";
                }
                else
                {
                    info.name = json_data.value("name", "N/A");
                    info.description = json_data.value("description", "No description");
                    info.stars = json_data.value("stargazers_count", 0);
                    info.forks = json_data.value("forks_count", 0);
                    info.open_issues = json_data.value("open_issues_count", 0);
                    info.html_url = json_data.value("html_url", "N/A");

                    // GitHub API returns the last push date as "pushed_at"
                    std::string pushed_at = json_data.value("pushed_at", "N/A");
                    if (pushed_at != "N/A" && pushed_at.length() >= 10)
                    {
                        info.last_commit_date = pushed_at.substr(0, 10); // Get only the date (YYYY-MM-DD)
                    }
                    else
                    {
                        info.last_commit_date = "N/A";
                    }
                    info.success = true; // Set to success if parsing succeeded
                }
            }
            catch (const nlohmann::json::exception &e)
            {
                info.error_message = "JSON parsing error: " + std::string(e.what());
            }
            catch (const std::exception &e)
            {
                info.error_message = "Unknown error: " + std::string(e.what());
            }
        }
        curl_easy_cleanup(curl);
    }
    else
    {
        info.error_message = "Failed to initialize cURL.";
    }
    return info;
}
