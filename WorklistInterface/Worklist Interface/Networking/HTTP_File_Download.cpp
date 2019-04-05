#include "HTTP_File_Download.h"

#include <stdexcept>	

#include "../Misc/StringConversions.h"

namespace ASAP
{
	boost::filesystem::path HTTP_File_Download(const web::http::http_response& response, const boost::filesystem::path& output_directory, std::string output_file, std::function<void(uint8_t)> observer)
	{
		// Fails if the path doesn't point towards a directory.
		if (!boost::filesystem::is_directory(output_directory))
		{
			// Replace with filesystem error once it has been moved out of experimental
			throw std::runtime_error("Defined directory isn't avaible or not a directory.");
		}

		// Fails if the response wasn't a HTTP 200 message, or lacks the content disposition header.
		web::http::http_headers headers(response.headers());
		auto content_length			= headers.find(L"Content-Length");
		if (response.status_code() == web::http::status_codes::OK && content_length != headers.end())
		{
			// Appends the filename to the output directory.
			boost::filesystem::path output_file(output_directory / boost::filesystem::path(output_file));
				
			// Checks if the file has already been downloaded.
			size_t length(std::stoi(content_length->second));
			if (FileIsUnique(output_file, length))
			{
				// Changes filename if the binary size is unique, but the filename isn't.
				FixFilepath(output_file);

				// Fails if the file can't be created and opened.
				concurrency::streams::ostream stream;
				concurrency::streams::fstream::open_ostream(output_file.wstring()).then([&stream](concurrency::streams::ostream open_stream)
				{
					stream = open_stream;
				}).wait();

				if (stream.is_open())
				{
					// Starts monitoring thread.
					bool finished = false;
					std::thread thread(StartMonitorThread(finished, length, stream, observer));
					response.body().read_to_end(stream.streambuf()).wait();
					stream.close().wait();
						
					// Joins monitoring thread.
					thread.join();

					if (FileHasCorrectSize(output_file, length))
					{
						return boost::filesystem::absolute(output_file);
					}
					throw std::runtime_error("Unable to complete download.");
				}

				// Replace with filesystem error once it has been moved out of experimental
				throw std::runtime_error("Unable to create file: " + output_file.string());
			}
			// File has already been downloaded.
			{
				return boost::filesystem::absolute(output_file);
			}
		}
		throw std::invalid_argument("HTTP Response contains no attachment.");
	}

	namespace
	{
		bool FileHasCorrectSize(const boost::filesystem::path& filepath, size_t size)
		{
			return boost::filesystem::exists(filepath) && boost::filesystem::file_size(filepath) == size;
		}

		bool FileIsUnique(const boost::filesystem::path& filepath, size_t size)
		{
			if (boost::filesystem::exists(filepath) && boost::filesystem::file_size(filepath) == size)
			{
				return false;
			}
			return true;
		}

		void FixFilepath(boost::filesystem::path& filepath)
		{
			while (boost::filesystem::exists(filepath))
			{
				std::string filename = filepath.leaf().string();

				size_t version			= 1;
				size_t version_location = filename.find('(');
				if (version_location != std::string::npos)
				{
					size_t value_start	= filename.find_last_of('(') + 1;
					size_t value_end	= filename.find_last_of(')');
					version = std::stoi(filename.substr(value_start, value_end - value_start)) + 1;
				}

				size_t dot_location = filename.find_first_of('.');
				std::string new_filename;
				if (version_location != std::string::npos || dot_location != std::string::npos)
				{
					size_t split_location	= version_location != std::string::npos ? version_location : dot_location;
					new_filename			= filename.substr(0, split_location) + "(" + std::to_string(version) + ")" + filename.substr(filename.find_first_of('.'));
				}
				else
				{
					new_filename = filename + "(" + std::to_string(version) + ")";
				}
				
				filepath.remove_leaf() /= new_filename;
			}
		}

		std::thread StartMonitorThread(const bool& stop, const size_t length, concurrency::streams::ostream& stream, std::function<void(uint8_t)>& observer)
		{
			return std::thread([&stop, length, &stream, observer](void)
			{
				// If there is no observer, we don't need to report progress.
				if (observer)
				{
					// Keeps checking progress until the stream is closed, or the download has completed.
					try
					{
						size_t percentile	= (length / 100);
						size_t progress		= stream.tell();
						while (!stop && progress < length)
						{
							observer(static_cast<float>(progress / percentile));
							std::this_thread::sleep_for(std::chrono::seconds(1));
							progress = stream.tell();
						}
					}
					catch (...)
					{
						// No need to handle this. If triggered, the stream has closed, and we no longer need to provide progress.
					}
				}
			});
		}
	}
}