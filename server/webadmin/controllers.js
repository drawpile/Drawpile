var dpserverApp = angular.module('dpserverApp', ['ngRoute', 'ngDialog']);

var APIROOT = "../api";

dpserverApp.config(function($routeProvider) {
	$routeProvider
		.when('/', {
			templateUrl: 'server-status.html',
			controller: 'ServerStatusController'
		})
		.when('/session/:sessionId', {
			templateUrl: 'session-status.html',
			controller: 'SessionStatusController'
		});
});

dpserverApp.controller('ServerStatusController', function($scope, $http, $interval, ngDialog) {
	function refreshData() {
		$http.get(APIROOT + '/status/').success(function(data) {
			$scope.status = data;
		});
		$http.get(APIROOT + '/sessions/').success(function(data) {
			$scope.sessions = data;
		});
	}

	refreshData();
	var refreshInterval = $interval(refreshData, 10000);
	$scope.$on('$destroy', function() {
		if (angular.isDefined(refreshInterval)) {
			$interval.cancel(refreshInterval);
			promise = undefined;
		}
	});

	$scope.showTitleChangeDialog = function() {
		var pscope = $scope;
		ngDialog.open({
			template: 'change-title.html',
			className: 'ngdialog-theme-plain',
			controller: function($scope) {
				$scope.title = pscope.status.title;
				$scope.save = function() {
					sendJson($http, APIROOT + '/status/', 'PUT', {
						'title': $scope.title
					}, function(data) { pscope.status = data; });
					$scope.closeThisDialog();
				};
			}
		});
	};

	$scope.showConfigDialog = function() {
		var pscope = $scope;
		ngDialog.open({
			template: 'config.html',
			className: 'ngdialog-theme-plain',
			controller: function($scope) {
				$scope.maxActiveSessions = pscope.status.maxActiveSessions;
				$scope.save = function() {
					sendJson($http, APIROOT + '/status/', 'PUT', {
						'maxActiveSessions': parseInt($scope.maxActiveSessions, 10),
					}, function(data) { pscope.status = data; });
					$scope.closeThisDialog();
				}
			}
		});
	};

	$scope.messageAll = function() {
		ngDialog.open({
			template: 'send-message.html',
			className: 'ngdialog-theme-plain',
			controller: function($scope) {
				$scope.message = "";
				$scope.save = function() {
					if($scope.message.length>0) {
						sendJson($http, APIROOT + '/wall/', 'POST', {
							'message': $scope.message
						});
						$scope.closeThisDialog();
					}
				};
			}
		});
	};
});


dpserverApp.controller('SessionStatusController', function($scope, $http, $routeParams, $location, $interval, ngDialog) {
	function refreshData() {
		$http.get(APIROOT + '/sessions/' + $routeParams.sessionId)
			.success(function(data) {
				$scope.status = data;
			}).error(function() {
				// session was probably deleted
				$location.path("/");
			});
	}

	refreshData();
	var refreshInterval = $interval(refreshData, 10000);
	$scope.$on('$destroy', function() {
		if (angular.isDefined(refreshInterval)) {
			$interval.cancel(refreshInterval);
			promise = undefined;
		}
	});

	$scope.messageSession = function() {
		ngDialog.open({
			template: 'send-message.html',
			className: 'ngdialog-theme-plain',
			controller: function($scope) {
				$scope.message = "";
				$scope.save = function() {
					if($scope.message.length>0) {
						sendJson($http, APIROOT + '/sessions/' + $routeParams.sessionId + '/wall/', 'POST', {
							'message': $scope.message
						});
						$scope.closeThisDialog();
					}
				};
			}
		});
	};

	$scope.killSession = function() {
		pscope = $scope;
		ngDialog.open({
			template: 'delete-session.html',
			className: 'ngdialog-theme-plain',
			controller: function($scope) {
				$scope.sessionId = pscope.status.id;
				$scope.kill = function() {
					$http.delete(APIROOT + '/sessions/' + $routeParams.sessionId).success(function() {
						$scope.closeThisDialog();
						$location.path("/");
					});
				};
			}
		});
	};

	$scope.kickUser = function(userId) {
		pscope = $scope;
		ngDialog.open({
			template: 'kick-user.html',
			className: 'ngdialog-theme-plain',
			controller: function($scope) {
				$scope.userId = userId;
				$scope.kick = function() {
					$http.delete(APIROOT + '/sessions/' + $routeParams.sessionId + "/user/" + userId).success(function() {
						$scope.closeThisDialog();
						refreshData();
					});
				};
			}
		});
	}
});

dpserverApp.directive('a', function() {
	return {
		restrict: 'E',
		link: function(scope, elem, attrs) {
			if(attrs.ngClick || attrs.href === '' || attrs.href === '#'){
				elem.on('click', function(e){
					e.preventDefault();
				});
			}
		}
	};
});

/* a helper function */
function sendJson(http, url, method, data, successfn)
{
	http({
		url: url,
		method: method,
		data: JSON.stringify(data),
		headers: {'Content-Type': 'application/json'}
	}).success(function(data) {
		if(data.success === false) {
			alert(data.error);
		} else if(successfn!=null) {
			successfn(data);
		}
	}).error(function() {
		alert("An error occurred.");
	});
}
