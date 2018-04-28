#!/usr/bin/env groovy

// Currently unused, but the goal would be to generate the stages from this.
def clang_builds = ["Linux && clang && LeakSanitizer",
                    "macOS && clang"]
def gcc_builds = ["Linux && gcc4.8",
                  "Linux && gcc4.9",
                  "Linux && gcc5.1",
                  "Linux && gcc6.3",
                  "Linux && gcc7.2"]
def msvc_builds = ["msbuild"]

// The options we use for the builds.
def gcc_cmake_opts = "-DCAF_NO_PROTOBUF_EXAMPLES:BOOL=yes " +
                     "-DCAF_NO_QT_EXAMPLES:BOOL=yes " +
                     "-DCAF_MORE_WARNINGS:BOOL=yes" +
                     "-DCAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes" +
                     "-DCAF_ENABLE_RUNTIME_CHECKS:BOOL=yes" +
                     "-DCAF_USE_ASIO:BOOL=yes" +
                     "-DCAF_NO_BENCHMARKS:BOOL=yes" +
                     "-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl" +
                     "-DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl"

def clang_cmake_opts = "-DCAF_NO_PROTOBUF_EXAMPLES:BOOL=yes " +
                       "-DCAF_NO_QT_EXAMPLES:BOOL=yes " +
                       "-DCAF_MORE_WARNINGS:BOOL=yes " +
                       "-DCAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes " +
                       "-DCAF_ENABLE_RUNTIME_CHECKS:BOOL=yes " +
                       "-DCAF_USE_ASIO:BOOL=yes " +
                       "-DCAF_NO_BENCHMARKS:BOOL=yes " +
                       "-DCAF_NO_OPENCL:BOOL=yes " +
                       "-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl " +
                       "-DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl"

def msbuild_opts = "-DCAF_BUILD_STATIC_ONLY:BOOL=yes " +
                   "-DCAF_NO_BENCHMARKS:BOOL=yes " +
                   "-DCAF_NO_EXAMPLES:BOOL=yes " +
                   "-DCAF_NO_MEM_MANAGEMENT:BOOL=yes " +
                   "-DCAF_NO_OPENCL:BOOL=yes " +
                   "-DCAF_LOG_LEVEL:INT=0 " +
                   "-DCMAKE_CXX_FLAGS=\"/MP\""

pipeline {
  agent none
  environment {
    LD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
    DYLD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
    ASAN_OPTIONS = 'detect_leaks=1'
  }
  stages {
    stage ('Git Checkout') {
      steps {
        node ('master') {
          deleteDir()
          dir('caf-sources') {
              checkout scm
          }
          stash includes: 'caf-sources/**', name: 'caf-sources'
        }
      }
    }
    stage ('Build & Test') {
      parallel {
        // gcc builds
        stage ("Linux && gcc4.8") {
          agent { label "Linux && gcc4.8" }
          steps { do_unix_stuff(gcc_cmake_opts) }
        }
        stage ("Linux && gcc4.9") {
          agent { label "Linux && gcc4.9" }
          steps { do_unix_stuff(gcc_cmake_opts) }
        }
        stage ("Linux && gcc5.1") {
          agent { label "Linux && gcc5.1" }
          steps { do_unix_stuff(gcc_cmake_opts) }
        }
        stage ("Linux && gcc6.3") {
          agent { label "Linux && gcc6.3" }
          steps { do_unix_stuff(gcc_cmake_opts) }
        }
        stage ("Linux && gcc7.2") {
          agent { label "Linux && gcc7.2" }
          steps { do_unix_stuff(gcc_cmake_opts) }
        }
        // clang builds
        stage ("macOS && clang") {
          agent { label "macOS && clang" }
          steps { do_unix_stuff(clang_cmake_opts) }
        }
        stage('Linux && clang && LeakSanitizer') {
          agent { label "Linux && clang && LeakSanitizer" }
          steps { do_unix_stuff(clang_cmake_opts) }
        }
        // windows builds
        stage('msbuild') {
          agent { label "msbuild" }
          steps { do_ms_stuff("msbuild", msbuild_opts) }
        }
      }
    }
  }
  post {
    success {
      echo "Success."
      // TODO: Gitter?
    }
    failure {
      echo "Failure, sending mails!"
      // TODO: Gitter?
      emailext(
        subject: "FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'",
        body: """FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]':
                 Check console output at "${env.BUILD_URL}"
                 Job: ${env.JOB_NAME}
                 Build number: ${env.BUILD_NUMBER}""",
        // TODO: Uncomment the following line:
        // recipientProviders: [[$class: 'CulpritsRecipientProvider'], [$class: 'RequesterRecipientProvider']]
        to: 'hiesgen' // add multiple separated with ';'
      )
    }
  }
}

def do_unix_stuff(cmake_opts = "",
                  build_type = "Debug",
                  generator = "Unix Makefiles",
                  build_opts = "",
                  clean_build = true) {
  deleteDir()
  unstash('caf-sources')
  dir('caf-sources') {
    // Configure and build.
    cmakeBuild([
      buildDir: 'build',
      buildType: "$build_type",
      cleanBuild: clean_build,
      cmakeArgs: "$cmake_opts",
      generator: "$generator",
      installation: 'cmake in search path',
      preloadScript: '../cmake/jenkins.cmake',
      sourceDir: '.',
      steps: [[args: 'all']],
    ])
    // Test.
    ctest([
      arguments: '--output-on-failure',
      installation: 'cmake auto install',
      workingDir: 'build',
    ])
  }
}

def do_ms_stuff(cmake_opts = "",
                build_type = "Debug",
                generator = "Visual Studio 15 2017",
                build_opts = "",
                clean_build = true) {
  withEnv(['PATH=C:\\Windows\\System32;C:\\Program Files\\CMake\\bin;C:\\Program Files\\Git\\cmd;C:\\Program Files\\Git\\bin']) {
    deleteDir()
    // TODO: pull from mirror, not from GitHub, (RIOT fetch func?)
    checkout scm
    // Configure and build.
    // installation can be either 'cmake auto install' or 'cmake in search path'
    // cmakeBuild buildDir: 'build', buildType: "$build_type", cleanBuild: clean_build, cmakeArgs: "$cmake_opts", generator: "$generator", installation: 'cmake in search path', preloadScript: '../cmake/jenkins.cmake', sourceDir: '.', steps: [[args: 'all']]
    def ret = bat(returnStatus: true,
              script: """cmake -E make_directory build
                         cd build
                         cmake -DCMAKE_BUILD_TYPE=${build_type} -G "${generator}" ${cmake_opts} ..
                         IF /I "%ERRORLEVEL%" NEQ "0" (
                           EXIT 1
                         )
                         cat build/CMakeCache.txt
                         EXIT 0""")
    if (ret) {
      echo "[!!!] Configure failed!"
      currentBuild.result = 'FAILURE'
      return
    }
    // bat "echo \"Step: Build for '${tags}'\""
    ret = bat(returnStatus: true,
              script: """cd build
                         cmake --build .
                         IF /I "%ERRORLEVEL%" NEQ "0" (
                           EXIT 1
                         )
                         EXIT 0""")
    if (ret) {
      echo "[!!!] Build failed!"
      currentBuild.result = 'FAILURE'
      return
    }
    // Test.
    ctest arguments: '--output-on-failure', installation: 'cmake auto install', workingDir: 'build'
  }
}
