var net = require('net');
var fs = require('fs');
var _ = require('lodash');

var client;
var helper = {};

helper.loginToAdminQuery= "USER LOGIN fzydb_admin password";
helper.sendCmd = function(cmd) {
  client.write(cmd);
  var beforeBytes = client.bytesRead;
  return new Promise(function(resolve, reject) {

    var resultString = "";

    var onDataFunc = function(data) {

      if((client.bytesRead-beforeBytes) < 1024) {
        //we are done - disconnect client and resolve promise
        client.removeListener('data', onDataFunc);
        resolve(JSON.parse(resultString + data));
      } else {
          var dataAsString = data.toString();

          for(var i = 0; i<data.length; i++) {
            if(data[i] != 0) {
              resultString += dataAsString[i];
            }
          }    

          if((client.bytesRead-beforeBytes) % 1024 != 0) {
            //we are done - disconnect client and resolve promise
            client.removeListener('data', onDataFunc);
            resolve(JSON.parse(resultString));
          }  

          beforeBytes = client.bytesRead;      
      }        
    };

    client.on('data', onDataFunc); 
  });     
};

//Assuming each requires admin 
helper.testCase = function(name, command, expected, timeout) {
  if(timeout == undefined) timeout = 1000;
  it(name+' -logging in as admin', function(done) {
      helper.sendCmd(helper.loginToAdminQuery).then(function(data) {
          expect(data.status).toBe(true);
          done();
      });
  });
  it(name, function(done) {
     helper.sendCmd(command)
    .then(function(data) {
      if(_.isFunction(expected)) {
        expected(data, done);
      } else {
        expect(data).toEqual(expected);
        done();
      }            
    }); 
  }, timeout);    
  it(name+' - logout from admin', function(done) {
      helper.sendCmd('USER LOGOUT').then(function(data) {
          expect(data.status).toBe(true);
          done();
      });
  });
};

helper.resultTemplate = function(results) {
  return {"status":true,"errorCode":0,"info":"","result":{"type":"fsparql","data":results}};
};

helper.setupClient = function(done) {
  client = new net.Socket();
  client.connect(1407, '127.0.0.1', function() {
    client.write("FLUSH");
    client.once('data', function(data) {
      done();
    });   
  });
}

helper.getData = function(name) {
  return fs.readFileSync("./spec/data/" + name, "utf8");
}

var admin_username = 'fzydb_admin';
var admin_pwd = 'password';
var space = ' ';


module.exports = helper;
