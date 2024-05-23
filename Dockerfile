FROM node AS base

# Install deps
RUN npm install -g http-server

# Copy the source files
COPY ./src/website/ /app/

# Serve the website
WORKDIR /app/
ENTRYPOINT http-server -p 80 -c-1
