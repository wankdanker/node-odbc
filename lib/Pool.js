const async = require('async');
const odbc = require('../build/Release/odbc.node');
const { Connection } = require('./Connection');

const INITIAL_SIZE_DEFAULT = 10;
const INCREMENT_SIZE_DEFAULT = 10;
const MAX_SIZE_DEFAULT = null;
const SHRINK_DEFAULT = true;
const CONNECTION_TIMEOUT_DEFAULT = 0;
const LOGIN_TIMEOUT_DEFAULT = 0;

class Pool {
  constructor(connectionString) {
    this.isOpen = false;
    this.freeConnections = [];

    // connectionString is a...
    if (typeof connectionString === 'string') {
      this.connectionString = connectionString;
      this.initialSize = INITIAL_SIZE_DEFAULT;
      this.incrementSize = INCREMENT_SIZE_DEFAULT;
      this.maxSize = MAX_SIZE_DEFAULT;
      this.shrink = SHRINK_DEFAULT;
      this.connectionTimeout = CONNECTION_TIMEOUT_DEFAULT;
      this.loginTimeout = LOGIN_TIMEOUT_DEFAULT;
    } else if (typeof connectionString === 'object') {
      const configObject = connectionString;
      if (!Object.prototype.hasOwnProperty.call(configObject, 'connectionString')) {
        throw new TypeError('Pool configuration object must contain "connectionString" key');
      }
      this.connectionString = configObject.connectionString;
      this.initialSize = configObject.initialSize || INITIAL_SIZE_DEFAULT;
      this.incrementSize = configObject.incrementSize || INCREMENT_SIZE_DEFAULT;
      this.maxSize = configObject.maxSize || MAX_SIZE_DEFAULT;
      this.shrink = configObject.shrink || SHRINK_DEFAULT;
      this.connectionTimeout = configObject.connectionTimeout || CONNECTION_TIMEOUT_DEFAULT;
      this.loginTimeout = configObject.loginTimeout || LOGIN_TIMEOUT_DEFAULT;
    } else {
      throw TypeError('Pool constructor must passed a connection string or a configuration object');
    }
  }

  // TODO: Documentation
  // TODO: Does this need to be async?
  // returns a open connection, ready to use.
  // should overwrite the 'close' function of the connection, and rename it is 'nativeClose', so
  // that that close can still be called.
  async connect(callback = undefined) {
    if (this.freeConnections.length < 2) {
      await this.increasePoolSize(this.incrementSize);
    }

    let connection;

    if (this.freeConnections.length > 0) {
      connection = this.freeConnections.pop();
    } else {
      await this.increasePoolSize(this.incrementSize);
      connection = this.freeConnections.pop();
    }

    connection.nativeClose = connection.close;

    connection.close = async (closeCallback = undefined) => {
      if (typeof closeCallback === 'undefined') {
        return new Promise((resolve, reject) => {
          connection.nativeClose((error, result) => {
            if (error) {
              reject(new Error(error));
            } else {
              resolve(result);
            }
          });

          this.increasePoolSize(1);
        });
      }

      connection.nativeClose(closeCallback);
      this.increasePoolSize(1);
      return undefined;
    };

    // promise...
    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        if (connection == null) {
          reject();
        } else {
          resolve(connection);
        }
      });
    }

    // ...or callback
    return callback(null, connection);
  }

  async query(sql, params, cb) {
    // determine the parameters passed
    let callback = cb;
    let parameters = params;

    if (typeof callback === 'undefined') {
      if (typeof parameters === 'function') {
        callback = parameters;
        parameters = null;
      } else if (typeof parameters === 'undefined') {
        parameters = null;
      } // else parameters = params, check type in this.ODBCconnection.query
    }

    let connection;

    if (this.freeConnections.length > 0) {
      connection = this.freeConnections.pop();
    } else {
      await this.increasePoolSize(this.incrementSize);
      connection = this.freeConnections.pop();
    }

    // promise...
    if (typeof callback !== 'function') {
      return new Promise((resolve, reject) => {
        connection.query(sql, parameters, (error, result) => {
          // after running, close the connection whether error or not
          connection.close(() => {
            this.increasePoolSize(1);
          });

          if (error) {
            reject(new Error(error));
          } else {
            resolve(result);
          }
        });
      });
    }

    // ...or callback
    return connection.query(sql, parameters, (error, result) => {
      // after running, close the connection whether error or not
      connection.close(() => {
        this.increasePoolSize(1);
      });
      callback(error, result);
    });
  }

  // close the pool and all of the connections
  async close(callback = undefined) {
    const connections = [...this.freeConnections];
    this.freeConnections.length = 0;
    this.isOpen = false;

    if (typeof callback === 'undefined') {
      return new Promise((resolve, reject) => {
        async.each(connections, (connection, cb) => {
          connection.close((error) => {
            cb(error);
          });
        }, (error) => {
          if (error) {
            reject(error);
          } else {
            resolve(error);
          }
        });
      });
    }

    async.each(this.freeConnections, (connection, cb) => {
      connection.close((error) => {
        cb(error);
      });
    }, error => callback(error));
  }

  async init(callback = undefined) {
    if (!this.isOpen) {
      this.isOpen = true;
      // promise...
      if (typeof callback === 'undefined') {
        return new Promise(async (resolve, reject) => {
          try {
            await this.increasePoolSize(this.initialSize);
            resolve(null);
          } catch (error) {
            reject(new Error(error));
          }
        });
      }

      // ...or callback
      try {
        await this.increasePoolSize(this.initialSize);
      } catch (error) {
        return callback(error);
      }
      return callback(null);
    }

    console.log('.init() was called, but the Pool was already initialized.');
    return undefined;
  }

  // odbc.connect runs on an AsyncWorker, so this is truly non-blocking
  async increasePoolSize(count) {
    const connectionConfig = {
      connectionString: this.connectionString,
      connectionTimeout: this.connectionTimeout,
      loginTimeout: this.loginTimeout,
    };
    return new Promise((resolve, reject) => {
      const connectArray = [];
      for (let i = 0; i < count; i += 1) {
        connectArray.push((callback) => {
          odbc.connect(connectionConfig, (error, connection) => {
            if (!error) {
              if (this.isOpen) {
                this.freeConnections.push(new Connection(connection));
              }
            }
            callback(error, connection);
          });
        });
      }
      async.race(connectArray, (error) => {
        if (error) {
          reject(error);
        } else {
          resolve();
        }
      });
    });
  }
}

module.exports.Pool = Pool;
