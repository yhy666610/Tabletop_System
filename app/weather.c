#include "weather.h"

bool parse_seniverse_response(const char *response, weather_info_t *info)
{
    response = strstr(response, "\"results\":"); // Find the start of the results array
	if (response == NULL)
		return false;

	const char *response_location = strstr(response, "\"location\":");
	if (response_location == NULL)
		return false;

    const char *response_name_location = strstr(response_location, "\"name\":");
	if (response_name_location)
	{
		sscanf(response_name_location, "\"name\": \"%31[^\"]\"", info->city); // Extract city name
	}

	const char *response_path_location = strstr(response_location, "\"path\":");
    if (response_path_location)
    {
        sscanf(response_path_location, "\"path\": \"%127[^\"]\"", info->location);
    }

    const char *response_now = strstr(response, "\"now\":");
    if (response_now == NULL)
        return false;

    const char *response_now_text = strstr(response_now, "\"text\":");
    if (response_now_text)
    {
        sscanf(response_now_text, "\"text\": \"%15[^\"]\"", info->weather);
    }

    const char *response_now_code = strstr(response_now, "\"code\":");
    if (response_now_code)
    {
        sscanf(response_now_code, "\"code\": \"%d\"", &info->weather_code);
    }

	char temperature_str[16] = { 0 };
    const char *response_now_temperature = strstr(response_now, "\"temperature\":");
    if (response_now_temperature)
    {
        if (sscanf(response_now_temperature, "\"temperature\": \"%15[^\"]\"", temperature_str) == 1)
        {
            info->temperature = atof(temperature_str);// Convert string to float
        }

    }

	return true;
}
